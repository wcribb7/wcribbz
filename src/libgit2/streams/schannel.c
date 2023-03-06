/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "streams/schannel.h"

#ifdef GIT_SCHANNEL

#define SECURITY_WIN32

#include <security.h>
#include <schannel.h>
#include <sspi.h>

#include "runtime.h"
#include "stream.h"
#include "streams/socket.h"

static void schannel_global_shutdown(void)
{
    WSACleanup();
}

int git_schannel_stream_global_init(void)
{
    WORD tls_version;
    WSADATA wsa_data;

    /* TODO: this is process global; allow callers to configure it so it doesn't overwrite their existing settings. */

    tls_version = MAKEWORD(2, 2);

    if (WSAStartup(tls_version, &wsa_data) != 0) {
	    git_error_set(GIT_ERROR_OS, "could not initialize Windows Socket Library");
	    return -1;
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
	    git_error_set(GIT_ERROR_SSL, "Windows Socket Library does not support Winsock 2.2");
	    return -1;
    }

    return git_runtime_shutdown_register(schannel_global_shutdown);
}

typedef enum {
    STATE_NONE = 0,
    STATE_CRED = 1,
    STATE_CONTEXT = 2,
} schannel_state;

typedef struct {
    git_stream parent;
    git_stream *io;
    int owned;
    bool connected;
    char *host;

    schannel_state state;

    CredHandle cred;
    CtxtHandle context;
    SecPkgContext_StreamSizes stream_sizes;

    git_str plaintext_in;
    git_str ciphertext_in;
} schannel_stream;

static int schannel_connect(git_stream *stream)
{
    schannel_stream *st = (schannel_stream *)stream;
    SCHANNEL_CRED cred = { 0 };
    SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
    DWORD context_flags;
    static size_t MAX_RETRIES = 1024;
    // TODO: sanity check this - probably 4096 is sane
    // to get it all in a single read. but 16k is the max tls packet size.
    static size_t READ_BLOCKSIZE = 4096;
    size_t retries;
    ssize_t read_len;
    int error = 0;

    if (st->owned && (error = git_stream_connect(st->io)) < 0)
	    return error;

    cred.dwVersion = SCHANNEL_CRED_VERSION;
    cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
    cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;
    cred.dwMinimumCipherStrength = 128; // TODO

    // TODO: handle st->state != 0

    /* TODO: do we need to pass an expiry timestamp? */
    if (AcquireCredentialsHandleW(NULL, SCHANNEL_NAME_W, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &st->cred, NULL) != SEC_E_OK) {
	    git_error_set(GIT_ERROR_OS, "could not acquire credentials handle");
	    return -1;
    }

    st->state = STATE_CRED;

    context_flags = ISC_REQ_ALLOCATE_MEMORY |
                        ISC_REQ_CONFIDENTIALITY |
                        ISC_REQ_REPLAY_DETECT |
                        ISC_REQ_SEQUENCE_DETECT |
                        ISC_REQ_STREAM;

    /* TODO: ensure that caller isn't reentering - we need st->context = NULL here */
    git_str_clear(&st->ciphertext_in);

    /* TODO: convert host to wchar and use W method */
    /* TODO: do we need to pass an expiry timestamp? (last arg) */
    for (retries = 0; retries < MAX_RETRIES; retries++) {
	    SecBuffer input_buf[] = { { (unsigned long)st->ciphertext_in.size, SECBUFFER_TOKEN, st->ciphertext_in.size ? st->ciphertext_in.ptr : NULL },
			              { 0, SECBUFFER_EMPTY, NULL } };
	    SecBuffer output_buf[] = { { 0, SECBUFFER_TOKEN, NULL },
			               { 0, SECBUFFER_ALERT, NULL } };

	    SecBufferDesc input_buf_desc = { SECBUFFER_VERSION, 2, input_buf };
	    SecBufferDesc output_buf_desc = { SECBUFFER_VERSION, 2,
			                      output_buf };

	    printf("input data: %d\n", st->ciphertext_in.size);

	    status = InitializeSecurityContextA(
		    &st->cred, retries ? &st->context : NULL, st->host,
		    context_flags, 0, 0, retries ? &input_buf_desc : NULL,
		    0, retries ? NULL : &st->context, &output_buf_desc,
		    &context_flags, NULL);


	    if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) {
		    st->state = STATE_CONTEXT;

		    if (output_buf[0].cbBuffer > 0) {
			    error = git_stream__write_full(
				    st->io, output_buf[0].pvBuffer,
				    output_buf[0].cbBuffer, 0);

			    FreeContextBuffer(output_buf[0].pvBuffer);
		    }



		/* handle any leftover, unprocessed data */
		    if (input_buf[1].BufferType == SECBUFFER_EXTRA) {
			    GIT_ASSERT(
				    st->ciphertext_in.size >
				    input_buf[1].cbBuffer);

			    printf("remain: %d\n", input_buf[1].cbBuffer);

			    git_str_consume_bytes(
				    &st->ciphertext_in,
				    st->ciphertext_in.size -
				            input_buf[1].cbBuffer);

			    printf("new len: %d\n", st->ciphertext_in.size);
		    } else {
			    printf("clearning...\n");
			    git_str_clear(&st->ciphertext_in);
			    printf("len: %d\n", st->ciphertext_in.size);
		    }





		    if (error < 0 || status == SEC_E_OK)
			    break;
	    } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
			    /* we need additional data from the client; */
			    if (git_str_grow_by(
				        &st->ciphertext_in, READ_BLOCKSIZE) <
				0) {
				    error = -1;
				    break;
			    }

			    printf(" pre-read: input data: %d\n",
				   st->ciphertext_in.size);

			    if ((read_len = git_stream_read(
				         st->io,
				         st->ciphertext_in.ptr +
				                 st->ciphertext_in.size,
				         (st->ciphertext_in.asize -
				          st->ciphertext_in.size))) < 0) {
				    error = -1;
				    break;
			    }

			    GIT_ASSERT(
				    (size_t)read_len <=
				    st->ciphertext_in.asize -
				            st->ciphertext_in.size);
			    st->ciphertext_in.size += read_len;
			    printf("     read: %d\n", read_len);
			    printf("post-read: input data: %d\n",
				   st->ciphertext_in.size);
		    
	    } else {
		    printf("yikes: %x\n", status);

		    git_error_set(
			    GIT_ERROR_OS,
			    "could not initialize security context");
		    error = -1;
		    break;
	    }

	    GIT_ASSERT(st->ciphertext_in.size < ULONG_MAX);
    }

    if (retries == MAX_RETRIES) {
	    git_error_set(
		    GIT_ERROR_SSL,
		    "could not initialize security context: too many retries");
	    error = -1;
    }

    if (!error) {
	if (QueryContextAttributesW(&st->context, SECPKG_ATTR_STREAM_SIZES, &st->stream_sizes) != SEC_E_OK) {
		    git_error_set(GIT_ERROR_SSL, "could not query stream sizes");
		error = -1;
	}
    }

    printf("error is: %d, status is: %d (%d)\n", error, status, (status == SEC_I_CONTINUE_NEEDED));

    st->connected = (error == 0);
    return error;
}

static int schannel_certificate(git_cert **out, git_stream *stream)
{
    return 0;
}

static int schannel_set_proxy(git_stream *stream, const git_proxy_options *proxy_options)
{
    schannel_stream *st = (schannel_stream *)stream;
    return git_stream_set_proxy(st->io, proxy_options);
}

static ssize_t
schannel_write(git_stream *stream, const char *data, size_t data_len, int flags)
{
    schannel_stream *st = (schannel_stream *)stream;
    SecBuffer encrypt_buf[3];
    SecBufferDesc encrypt_buf_desc = { SECBUFFER_VERSION, 3, encrypt_buf };
    ssize_t total_len = 0;

    /* TODO: use a git_str on the stream */
    git_str ciphertext_out = GIT_STR_INIT;

    GIT_UNUSED(flags);

    if (data_len > SSIZE_MAX)
	data_len = SSIZE_MAX;


    // TODO: should this move?
    git_str_init(&ciphertext_out, st->stream_sizes.cbHeader + st->stream_sizes.cbMaximumMessage + st->stream_sizes.cbTrailer);


    while (data_len > 0) {
	size_t message_len = min(data_len, st->stream_sizes.cbMaximumMessage);
	size_t ciphertext_len, ciphertext_written = 0;

	encrypt_buf[0].BufferType = SECBUFFER_STREAM_HEADER;
	encrypt_buf[0].cbBuffer = st->stream_sizes.cbHeader;
	encrypt_buf[0].pvBuffer = ciphertext_out.ptr;

	encrypt_buf[1].BufferType = SECBUFFER_DATA;
	encrypt_buf[1].cbBuffer = (unsigned long)message_len;
	encrypt_buf[1].pvBuffer =
		ciphertext_out.ptr + st->stream_sizes.cbHeader;

	encrypt_buf[2].BufferType = SECBUFFER_STREAM_TRAILER;
	encrypt_buf[2].cbBuffer = st->stream_sizes.cbTrailer;
	encrypt_buf[2].pvBuffer =
		ciphertext_out.ptr + st->stream_sizes.cbHeader + message_len;

	memcpy(ciphertext_out.ptr + st->stream_sizes.cbHeader, data, message_len);

	if (EncryptMessage(&st->context, 0, &encrypt_buf_desc, 0) != SEC_E_OK) {
		git_error_set(GIT_ERROR_OS, "could not encrypt tls message");
		total_len = -1;
		goto done;
	}

	ciphertext_len = encrypt_buf[0].cbBuffer + encrypt_buf[1].cbBuffer + encrypt_buf[2].cbBuffer;

	while (ciphertext_written < ciphertext_len) {
	    ssize_t chunk_len = git_stream_write(st->io, ciphertext_out.ptr + ciphertext_written, ciphertext_len - ciphertext_written, 0);

	    if (chunk_len < 0) {
		total_len = -1;
		goto done;
	    }

	    ciphertext_len -= chunk_len;
	    ciphertext_written += chunk_len;
	}

	total_len += message_len;

	data += message_len;
	data_len -= message_len;
    }

done:
    return total_len;
}

static ssize_t schannel_read(git_stream *stream, void *_data, size_t data_len)
{
    schannel_stream *st = (schannel_stream *)stream;
    char *data = (char *)_data;
    SecBuffer decrypt_buf[4];
    SecBufferDesc decrypt_buf_desc = { SECBUFFER_VERSION, 4, decrypt_buf };
    SECURITY_STATUS status;
    const size_t READ_BLOCKSIZE = (16 * 1024);
    bool has_read = false;
    ssize_t chunk_len, total_len = 0;

    if (data_len > SSIZE_MAX)
	data_len = SSIZE_MAX;

    while ((size_t)total_len < data_len) {
	if (st->plaintext_in.size > 0) {
	    size_t copy_len = min(st->plaintext_in.size, data_len);

	    memcpy(data, st->plaintext_in.ptr, copy_len);
	    git_str_consume_bytes(&st->plaintext_in, copy_len);

	    data += copy_len;
	    data_len -= copy_len;

	    total_len += copy_len;

	    continue;
	}

	if (st->ciphertext_in.size > 0) {
	    decrypt_buf[0].BufferType = SECBUFFER_DATA;
	    decrypt_buf[0].cbBuffer = (unsigned long)min(st->ciphertext_in.size, ULONG_MAX);
	    decrypt_buf[0].pvBuffer = st->ciphertext_in.ptr;

	    decrypt_buf[1].BufferType = SECBUFFER_EMPTY;
	    decrypt_buf[1].cbBuffer = 0;
	    decrypt_buf[1].pvBuffer = NULL;

	    decrypt_buf[2].BufferType = SECBUFFER_EMPTY;
	    decrypt_buf[2].cbBuffer = 0;
	    decrypt_buf[2].pvBuffer = NULL;

	    decrypt_buf[3].BufferType = SECBUFFER_EMPTY;
	    decrypt_buf[3].cbBuffer = 0;
	    decrypt_buf[3].pvBuffer = NULL;

	    status = DecryptMessage(&st->context, &decrypt_buf_desc, 0, NULL);

	    if (status == SEC_E_OK) {
		GIT_ASSERT(decrypt_buf[0].BufferType == SECBUFFER_STREAM_HEADER);
		GIT_ASSERT(decrypt_buf[1].BufferType == SECBUFFER_DATA);
		GIT_ASSERT(decrypt_buf[2].BufferType == SECBUFFER_STREAM_TRAILER);

		if (git_str_put(&st->plaintext_in, decrypt_buf[1].pvBuffer, decrypt_buf[1].cbBuffer) < 0) {
			total_len = -1;
			goto done;
		}

		if (decrypt_buf[3].BufferType == SECBUFFER_EXTRA) {
			git_str_consume_bytes(&st->ciphertext_in, (st->ciphertext_in.size - decrypt_buf[3].cbBuffer));
		} else {
			git_str_clear(&st->ciphertext_in);
		}

		continue;
	    } else if (status == SEC_E_CONTEXT_EXPIRED) {
		break;
	    } else if (status != SEC_E_INCOMPLETE_MESSAGE) {
		git_error_set(GIT_ERROR_SSL, "could not decrypt tls message");
		total_len = -1;
		goto done;
	    }
	}

	if (!has_read) {
	    if (git_str_grow_by(&st->ciphertext_in, READ_BLOCKSIZE) < 0) {
		total_len = -1;
		goto done;
	    }

	    if ((chunk_len = git_stream_read(st->io, st->ciphertext_in.ptr + st->ciphertext_in.size, st->ciphertext_in.asize - st->ciphertext_in.size)) < 0) {
		total_len = -1;
		goto done;
	    }

	    st->ciphertext_in.size += chunk_len;

	    has_read = true;
	    continue;
	}

	break;
    }

done:
    return total_len;
}

static int schannel_close(git_stream *stream)
{
    schannel_stream *st = (schannel_stream *)stream;
    int error = 0;

    if (st->connected) {
	SecBuffer shutdown_buf;
	SecBufferDesc shutdown_buf_desc = { SECBUFFER_VERSION, 1,
			                    &shutdown_buf };
	DWORD shutdown_message = SCHANNEL_SHUTDOWN, shutdown_flags;

	shutdown_buf.BufferType = SECBUFFER_TOKEN;
	shutdown_buf.cbBuffer = sizeof(DWORD);
	shutdown_buf.pvBuffer = &shutdown_message;

	if (ApplyControlToken(&st->context, &shutdown_buf_desc) != SEC_E_OK) {
	    git_error_set(GIT_ERROR_SSL, "could not shutdown stream");
	    error = -1;
	}

	shutdown_buf.BufferType = SECBUFFER_TOKEN;
	shutdown_buf.cbBuffer = 0;
	shutdown_buf.pvBuffer = NULL;

	shutdown_flags = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY |
		         ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
		         ISC_REQ_STREAM;

	if (InitializeSecurityContext(
		    &st->cred, &st->context, NULL, shutdown_flags, 0, 0,
		    &shutdown_buf_desc, 0, NULL, &shutdown_buf_desc,
		    &shutdown_flags, NULL) == SEC_E_OK) {
	    if (shutdown_buf.cbBuffer > 0) {
		if (git_stream__write_full(
			    st->io, shutdown_buf.pvBuffer,
			    shutdown_buf.cbBuffer, 0) < 0)
			error = -1;

		FreeContextBuffer(shutdown_buf.pvBuffer);
	    }
	}
    }

    st->connected = false;

    if (st->owned && git_stream_close(st->io) < 0)
	error = -1;

    return error;
}

static void schannel_free(git_stream *stream)
{
    schannel_stream *st = (schannel_stream *)stream;

    if (st->state >= STATE_CONTEXT)
	DeleteSecurityContext(&st->context);

    if (st->state >= STATE_CRED)
	FreeCredentialsHandle(&st->cred);

    st->state = STATE_NONE;

    git_str_dispose(&st->ciphertext_in);
    git_str_dispose(&st->plaintext_in);

    git__free(st->host);
    git__free(st);
}

static int schannel_stream_wrap(
        git_stream **out,
        git_stream *in,
        const char *host,
        int owned)
{
    schannel_stream *st;

    st = git__calloc(1, sizeof(schannel_stream));
    GIT_ERROR_CHECK_ALLOC(st);

    st->io = in;
    st->owned = owned;

    st->host = git__strdup(host);
    GIT_ERROR_CHECK_ALLOC(st->host);

    st->parent.version = GIT_STREAM_VERSION;
    st->parent.encrypted = 1;
    st->parent.proxy_support = git_stream_supports_proxy(st->io);
    st->parent.connect = schannel_connect;
    st->parent.certificate = schannel_certificate;
    st->parent.set_proxy = schannel_set_proxy;
    st->parent.read = schannel_read;
    st->parent.write = schannel_write;
    st->parent.close = schannel_close;
    st->parent.free = schannel_free;

    *out = (git_stream *)st;
    return 0;
}

extern int git_schannel_stream_new(git_stream **out, const char *host, const char *port)
{
    git_stream *stream;
    int error;

    GIT_ASSERT_ARG(out);
    GIT_ASSERT_ARG(host);
    GIT_ASSERT_ARG(port);

    if ((error = git_socket_stream_new(&stream, host, port)) < 0)
	    return error;

    if ((error = schannel_stream_wrap(out, stream, host, 1)) < 0) {
	    git_stream_close(stream);
	    git_stream_free(stream);
    }

    return error;
}

extern int git_schannel_stream_wrap(git_stream **out, git_stream *in, const char *host)
{
    return schannel_stream_wrap(out, in, host, 0);
}

#endif
