/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_rand_h__
#define INCLUDE_rand_h__

#include "common.h"

int git_rand_global_init(void);
void git_rand_seed(uint64_t seed);
uint64_t git_rand_next(void);

#endif
