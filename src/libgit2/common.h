/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "git2_util.h"
#include "errors.h"

/*
* Include the declarations for deprecated functions; this ensures
* that they're decorated with the proper extern/visibility attributes.
*/
#include "git2/deprecated.h"

#include "posix.h"

/**
 * Check a versioned structure for validity
 */
GIT_INLINE(int) git_error__check_version(const void *structure, unsigned int expected_max, const char *name)
{
	unsigned int actual;

	if (!structure)
		return 0;

	actual = *(const unsigned int*)structure;
	if (actual > 0 && actual <= expected_max)
		return 0;

	git_error_set(GIT_ERROR_INVALID, "invalid version %d on %s", actual, name);
	return -1;
}
#define GIT_ERROR_CHECK_VERSION(S,V,N) if (git_error__check_version(S,V,N) < 0) return -1

#endif
