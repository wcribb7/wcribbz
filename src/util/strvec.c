/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "strvec.h"

GIT_INLINE(int) copy_strings(git_strvec *out, const char **in, size_t len)
{
	size_t i;

	GIT_ERROR_CHECK_ALLOC(out->ptr);

	for (i = 0; i < len; i++) {
		out->ptr[i] = git__strdup(in[i]);
		GIT_ERROR_CHECK_ALLOC(out->ptr[i]);
	}

	out->len = len;
	return 0;
}

int git_strvec_copy_strings(git_strvec *out, const char **in, size_t len)
{
	out->ptr = git__calloc(len, sizeof(char *));

	return copy_strings(out, in, len);
}

int git_strvec_copy_strings_with_null(
	git_strvec *out,
	const char **in,
	size_t len)
{
	size_t new_len;

	GIT_ERROR_CHECK_ALLOC_ADD(&new_len, len, 1);

	if (copy_strings(out, in, len) < 0)
		return -1;

	out->len = new_len;
	return 0;
}

bool git_strvec_contains_prefix(
	const git_strvec *strings,
	const char *str,
	size_t n)
{
	size_t i;

	for (i = 0; i < strings->len; i++) {
		if (strncmp(strings->ptr[i], str, n) == 0)
			return true;
	}

	return false;
}

bool git_strvec_contains_key(
	const git_strvec *strings,
	const char *key,
	char delimiter)
{
	const char *c;

	for (c = key; *c; c++) {
		if (*c == delimiter)
			break;
	}

	return *c ?
	       git_strvec_contains_prefix(strings, key, (c - key)) :
	       false;
}

void git_strvec_dispose(git_strvec *strings)
{
	size_t i;

	if (!strings)
		return;

	for (i = 0; i < strings->len; i++)
		git__free(strings->ptr[i]);

	git__free(strings->ptr);
}
