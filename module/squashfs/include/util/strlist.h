/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * strlist.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_STRLIST_H
#define UTIL_STRLIST_H

#include "sqfs/predef.h"
#include <string.h>

typedef struct {
	char **strings;
	size_t count;
	size_t capacity;
} strlist_t;

static SQFS_INLINE void strlist_init(strlist_t *list)
{
	memset(list, 0, sizeof(*list));
}

#ifdef __cplusplus
extern "C" {
#endif

SQFS_INTERNAL int strlist_init_copy(strlist_t *dst, const strlist_t *src);

SQFS_INTERNAL void strlist_cleanup(strlist_t *list);

SQFS_INTERNAL int strlist_append(strlist_t *list, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_STRLIST_H */
