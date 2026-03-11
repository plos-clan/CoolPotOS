/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * str_table.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef STR_TABLE_H
#define STR_TABLE_H

#include "sqfs/predef.h"
#include "array.h"
#include "hash_table.h"

typedef struct {
	size_t index;
	size_t refcount;
	char string[];
} str_bucket_t;

/* Stores strings in a hash table and assigns an incremental, unique ID to
   each string. Subsequent additions return the existing ID. The ID can be
   used for (hopefully) constant time lookup of the original string. */
typedef struct {
	/* an array that resolves index to bucket pointer */
	array_t bucket_ptrs;

	/* hash table with the string buckets attached */
	struct hash_table *ht;

	/* the next ID we are going to allocate */
	size_t next_index;
} str_table_t;

/* the number of strings currently stored in the table */
static SQFS_INLINE size_t str_table_count(const str_table_t *table)
{
	return table->next_index;
}

/* `size` is the number of hash table buckets to use internally. */
SQFS_INTERNAL int str_table_init(str_table_t *table);

SQFS_INTERNAL void str_table_cleanup(str_table_t *table);

SQFS_INTERNAL int str_table_copy(str_table_t *dst, const str_table_t *src);

/* Resolve a string to an incremental, unique ID. */
SQFS_INTERNAL
int str_table_get_index(str_table_t *table, const char *str, size_t *idx);

/* Resolve a unique ID to the string it represents.
   Returns NULL if the ID is unknown, i.e. out of bounds. */
SQFS_INTERNAL
const char *str_table_get_string(const str_table_t *table, size_t index);

SQFS_INTERNAL void str_table_add_ref(str_table_t *table, size_t index);

SQFS_INTERNAL void str_table_del_ref(str_table_t *table, size_t index);

SQFS_INTERNAL
size_t str_table_get_ref_count(const str_table_t *table, size_t index);

#endif /* STR_TABLE_H */
