/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * array.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef ARRAY_H
#define ARRAY_H

#include "sqfs/predef.h"
#include "sqfs/error.h"

typedef struct {
	/* sizeof a single element */
	size_t size;

	/* total number of elements available */
	size_t count;

	/* actually used number of elements available */
	size_t used;

	void *data;
} array_t;

static SQFS_INLINE void *array_get(array_t *array, size_t index)
{
	if (index >= array->used)
		return NULL;

	return (char *)array->data + array->size * index;
}

static SQFS_INLINE int array_set(array_t *array, size_t index, const void *data)
{
	if (index >= array->used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	memcpy((char *)array->data + array->size * index, data, array->size);
	return 0;
}

static SQFS_INLINE void array_sort_range(array_t *array, size_t start,
					 size_t count,
					 int (*compare_fun)(const void *a,
							    const void *b))
{
	if (start < array->used) {
		if (count > (array->used - start))
			count = array->used - start;

		qsort((char *)array->data + array->size * start, count,
		      array->size, compare_fun);
	}
}

#ifdef __cplusplus
extern "C" {
#endif

SQFS_INTERNAL int array_init(array_t *array, size_t size, size_t capacity);

SQFS_INTERNAL int array_init_copy(array_t *array, const array_t *src);

SQFS_INTERNAL void array_cleanup(array_t *array);

SQFS_INTERNAL int array_append(array_t *array, const void *data);

SQFS_INTERNAL int array_set_capacity(array_t *array, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* ARRAY_H */
