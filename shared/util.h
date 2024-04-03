// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WESTON_UTIL_H
#define WESTON_UTIL_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

static inline void *zalloc(size_t size)
{
	return calloc(1, size);
}

bool
safe_strtoint(const char *str, int32_t *value);

#endif
