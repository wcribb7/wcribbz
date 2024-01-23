/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "packfile_parser.h"

#include "oid.h"
#include "pack.h"
#include "repository.h"

#include "git2_util.h"

/* TODO: tweak? */
#define READ_CHUNK_SIZE (1024 * 256)

int git_packfile_parser_init(
	git_packfile_parser *parser,
	git_oid_t oid_type)
{
	git_hash_algorithm_t checksum_type = git_oid_algorithm(oid_type);

	GIT_ASSERT_ARG(parser);

	memset(parser, 0x0, sizeof(git_packfile_parser));

	parser->oid_type = oid_type;

	if (git_hash_ctx_init(&parser->current_hash, checksum_type) < 0 ||
	    git_hash_ctx_init(&parser->packfile_hash, checksum_type) < 0)
		return -1;

	return 0;
}

static int parse_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	size_t chunk_len = min(len,
		(sizeof(struct git_pack_header) - parser->header_len));
	size_t orig_len = len;
	unsigned char *header = (unsigned char *)&parser->header;

	memcpy(header + parser->header_len, data, chunk_len);

	data += chunk_len;
	len -= chunk_len;
	parser->header_len += chunk_len;

	if (parser->header_len == sizeof(struct git_pack_header)) {
		parser->header.hdr_signature = ntohl(parser->header.hdr_signature);
		parser->header.hdr_version = ntohl(parser->header.hdr_version);
		parser->header.hdr_entries = ntohl(parser->header.hdr_entries);

		if (parser->header.hdr_signature != PACK_SIGNATURE) {
			git_error_set(GIT_ERROR_INDEXER, "invalid packfile signature");
			return -1;
		}

		if (parser->header.hdr_version != 2) {
			git_error_set(GIT_ERROR_INDEXER, "unsupported packfile version %d", parser->header.hdr_version);
			return -1;
		}

		if (parser->packfile_header) {
			int error = parser->packfile_header(parser->header.hdr_version,
				parser->header.hdr_entries,
				parser->callback_data);

			if (error != 0)
				return error;
		}

		parser->state = parser->header.hdr_entries > 0 ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

#define is_delta(type) \
	((type) == GIT_OBJECT_OFS_DELTA || (type) == GIT_OBJECT_REF_DELTA)

static int parse_object_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	size_t orig_len = len;

	while (len && parser->state < STATE_OBJECT_DATA_START) {
		unsigned char c = *((unsigned char *)data);

		if (parser->state == STATE_OBJECT_HEADER_START) {
			parser->state = STATE_OBJECT_HEADER;
			parser->current_position = parser->position;
			parser->current_type = (c >> 4) & 0x07;
			parser->current_size = c & 0x0f;
			parser->current_compressed_size = 1;
			parser->current_compressed_crc = crc32(0L, Z_NULL, 0);
			parser->current_bits = 4;

			if (git_hash_init(&parser->current_hash) < 0)
				return -1;
		} else {
			parser->current_size += (c & 0x7f) << parser->current_bits;
			parser->current_compressed_size++;
			parser->current_bits += 7;
		}

		parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, 1);

		data++;
		len--;

		if ((c & 0x80) == 0 && is_delta(parser->current_type)) {
			parser->current_offset = 0;
			git_oid_clear(&parser->current_base, parser->oid_type);
			parser->current_base_len = 0;
			parser->current_bits = 0;

			parser->state = STATE_DELTA_HEADER;
		} else if ((c & 0x80) == 0) {
			/* TODO: constant somewhere? */
			char header[64];
			size_t header_len;

			if (git_odb__format_object_header(&header_len,
					header, sizeof(header),
					parser->current_size,
					parser->current_type) < 0 ||
			    git_hash_update(&parser->current_hash,
					header, header_len) < 0)
				return -1;

			if (parser->object_start) {
				int error = parser->object_start(
					parser->current_position,
					parser->current_compressed_size,
					parser->current_type,
					parser->current_size,
					parser->callback_data);

				if (error != 0)
					return error;
			}

			parser->state = STATE_OBJECT_DATA_START;
		}
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_object_data(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	unsigned char inflated[READ_CHUNK_SIZE];
	size_t inflated_size = READ_CHUNK_SIZE, inflated_len;
	size_t orig_len = len;

	if (parser->state == STATE_OBJECT_DATA_START) {
		if (git_zstream_init(&parser->zstream, GIT_ZSTREAM_INFLATE) < 0)
			return -1;

		parser->state = STATE_OBJECT_DATA;
	}

	if (git_zstream_set_input(&parser->zstream, data, len) < 0)
		return -1;

	do {
		inflated_len = inflated_size;

		if (git_zstream_get_output_chunk(inflated, &inflated_len, &parser->zstream) < 0 ||
		    git_hash_update(&parser->current_hash, inflated, inflated_len) < 0)
			return -1;

		if (parser->object_data) {
			int error = parser->object_data(
				inflated,
				inflated_len,
				parser->callback_data);

			if (error != 0)
				return error;
		}
	} while (inflated_len > 0);

	len = parser->zstream.in_len;
	parser->current_compressed_size += (orig_len - len);
	parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, (orig_len - len));

	if (git_zstream_eos(&parser->zstream)) {
		git_oid oid = {0};

#ifdef GIT_EXPERIMENTAL_SHA256
		oid.type = parser->oid_type;
#endif

		/* TODO: sanity check length */

		if (git_hash_final(oid.id, &parser->current_hash) < 0)
			return -1;

		if (parser->object_complete) {
			int error = parser->object_complete(
				parser->current_compressed_size,
				parser->current_compressed_crc,
				&oid,
				parser->callback_data);

			if (error != 0)
				return error;
		}

		parser->state =
			(++parser->current_idx < parser->header.hdr_entries) ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_delta_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	size_t hash_len, chunk_len;
	size_t orig_len = len;
	unsigned char *base;

	switch (parser->current_type) {
	case GIT_OBJECT_OFS_DELTA:
		printf("i'm an offset delta and my crc is %lx\n", parser->current_compressed_crc);
		while (len) {
			char c = *((const char *)data);

			if (parser->current_bits == 0) {
				parser->current_offset = (c & 0x7f);
			} else {
				parser->current_offset += 1;
				parser->current_offset <<= 7;
				parser->current_offset |= (c & 0x7f);
			}

			/* TODO: overflow checking */
			parser->current_bits += 7;

			parser->current_compressed_size++;

			parser->current_compressed_crc =
				crc32(parser->current_compressed_crc, data, 1);

			data++;
			len--;

			if ((c & 0x80) == 0) {
				if (parser->delta_start) {
					int error = parser->delta_start(
						parser->current_position,
						parser->current_type,
						parser->current_compressed_size,
						parser->current_size,
						NULL,
						parser->current_offset,
						parser->callback_data);

					if (error != 0)
						return error;
				}

				parser->state = STATE_DELTA_DATA_START;
				break;
			}
		}

		break;

	case GIT_OBJECT_REF_DELTA:
			printf("i'm a ref delta and my crc is %lx\n", parser->current_compressed_crc);

		hash_len = git_oid_size(parser->oid_type);
		chunk_len = min(hash_len, len);

		base = (unsigned char *)&parser->current_base.id;
		memcpy(base + parser->current_base_len, data, chunk_len);

		parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, chunk_len);

		parser->current_base_len += chunk_len;
		data += chunk_len;
		len -= chunk_len;

		parser->current_compressed_size += chunk_len;

		if (parser->current_base_len == hash_len) {
			if (parser->delta_start) {
				int error = parser->delta_start(
					parser->current_position,
					parser->current_type,
					parser->current_compressed_size,
					parser->current_size,
					&parser->current_base,
					0,
					parser->callback_data);

				if (error != 0)
					return error;
			}

			parser->state = STATE_DELTA_DATA_START;
		}

		break;

	default:
		git_error_set(GIT_ERROR_INDEXER, "invalid delta type");
		return -1;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_delta_data(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	unsigned char inflated[READ_CHUNK_SIZE];
	size_t inflated_len = READ_CHUNK_SIZE;
	size_t orig_len = len;

	if (parser->state == STATE_DELTA_DATA_START) {
		if (git_zstream_init(&parser->zstream, GIT_ZSTREAM_INFLATE) < 0)
			return -1;

		parser->state = STATE_DELTA_DATA;
	}

	if (git_zstream_set_input(&parser->zstream, data, len) < 0 ||
	    git_zstream_get_output_chunk(inflated, &inflated_len, &parser->zstream) < 0)
		return -1;

	len = parser->zstream.in_len;
	parser->current_compressed_size += (orig_len - len);

	parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, (orig_len - len));

	if (parser->delta_data) {
		int error = parser->delta_data(
			inflated,
			inflated_len,
			parser->callback_data);

		if (error != 0)
			return error;
	}

	if (git_zstream_eos(&parser->zstream)) {
		if (parser->delta_complete) {
			int error = parser->delta_complete(
				parser->current_compressed_size,
				parser->current_compressed_crc,
				parser->callback_data);

			if (error != 0)
				return error;
		}

		parser->state =
			(++parser->current_idx < parser->header.hdr_entries) ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_trailer(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	git_hash_algorithm_t hash_alg = git_oid_algorithm(parser->oid_type);
	size_t hash_len = git_hash_size(hash_alg);
	size_t chunk_len = min(hash_len, len);
	size_t orig_len = len;

	memcpy(parser->trailer + parser->trailer_len, data, chunk_len);
	parser->trailer_len += chunk_len;

	len -= chunk_len;

	if (parser->trailer_len == hash_len) {
		unsigned char trailer[GIT_HASH_MAX_SIZE];

		git_hash_final(trailer, &parser->packfile_hash);

		if (memcmp(trailer, parser->trailer, parser->trailer_len) != 0) {
			git_error_set(GIT_ERROR_INDEXER, "incorrect packfile checksum");
			return -1;
		}

		if (parser->packfile_complete) {
			int error = parser->packfile_complete(
				trailer,
				hash_len,
				parser->callback_data);

			if (error != 0)
				return error;
		}

		parser->state = STATE_COMPLETE;
	}

	*out = (orig_len - len);
	return 0;
}

int git_packfile_parser_parse(
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	GIT_ASSERT_ARG(parser && (!len || data));

	while (len) {
		parser_state start_state = parser->state;
		size_t consumed;
		int error = 0;

		if (len == 20)
			printf("here\n");
			
		printf("consuming up to %d bytes - state is %d (%d / %d)\n", (int)len, parser->state, STATE_TRAILER, STATE_COMPLETE);

		switch (parser->state) {
		case STATE_HEADER:
			error = parse_header(&consumed, parser, data, len);
			break;
		case STATE_OBJECT_HEADER_START:
		case STATE_OBJECT_HEADER:
			error = parse_object_header(&consumed, parser, data, len);
			break;
		case STATE_DELTA_HEADER:
			error = parse_delta_header(&consumed, parser, data, len);
			break;
		case STATE_OBJECT_DATA_START:
		case STATE_OBJECT_DATA:
			error = parse_object_data(&consumed, parser, data, len);
			break;
		case STATE_DELTA_DATA_START:
		case STATE_DELTA_DATA:
			error = parse_delta_data(&consumed, parser, data, len);
			break;
		case STATE_TRAILER:
			error = parse_trailer(&consumed, parser, data, len);
			break;
		case STATE_COMPLETE:
			git_error_set(GIT_ERROR_INDEXER, "packfile data after completion");
			return -1;
		default:
			GIT_ASSERT(!parser->state);
		}

		if (error == 0 && start_state < STATE_TRAILER)
			error = git_hash_update(&parser->packfile_hash, data, consumed);

		if (error != 0) {
			printf("FAILED\n");
			parser->state = STATE_FAILED;
			return error;
		}

		parser->position += consumed;
		data += consumed;
		len -= consumed;
	}

	return 0;
}

int git_packfile_parser_complete(git_packfile_parser *parser)
{
	printf("state is: %d (expected: %d)\n", parser->state, STATE_COMPLETE);
	return 0;
}

void git_packfile_parser_dispose(git_packfile_parser *parser)
{
	if (!parser)
		return;

	git_hash_ctx_cleanup(&parser->current_hash);
	git_hash_ctx_cleanup(&parser->packfile_hash);
}
