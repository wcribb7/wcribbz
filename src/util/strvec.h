/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_strvec_h__
#define INCLUDE_strvec_h__

#include "git2_util.h"

typedef struct {
	char **ptr;
	size_t len;
} git_strvec;

extern int git_strvec_copy_strings(
	git_strvec *out,
	const char **in,
	size_t len);

extern int git_strvec_copy_strings_with_null(
	git_strvec *out,
	const char **in,
	size_t len);

extern bool git_strvec_contains_prefix(
	const git_strvec *strings,
	const char *str,
	size_t n);

extern bool git_strvec_contains_key(
	const git_strvec *strings,
	const char *key,
	char delimiter);

extern void git_strvec_dispose(git_strvec *strings);

#endif
