/*
 * Copyright © 2009,2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _HASH_TABLE_H
#define _HASH_TABLE_H

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "sqfs/predef.h"

struct hash_entry {
   sqfs_u32 hash;
   const void *key;
   void *data;
};

struct hash_table {
   struct hash_entry *table;
   sqfs_u32 (*key_hash_function)(void *user, const void *key);
   bool (*key_equals_function)(void *user, const void *a, const void *b);
   const void *deleted_key;
   void *user;
   sqfs_u32 size;
   sqfs_u32 rehash;
   sqfs_u64 size_magic;
   sqfs_u64 rehash_magic;
   sqfs_u32 max_entries;
   sqfs_u32 size_index;
   sqfs_u32 entries;
   sqfs_u32 deleted_entries;
};

SQFS_INTERNAL struct hash_table *
hash_table_create(sqfs_u32 (*key_hash_function)(void *user, const void *key),
                  bool (*key_equals_function)(void *user, const void *a,
                                              const void *b));

SQFS_INTERNAL struct hash_table *
hash_table_clone(struct hash_table *src);

SQFS_INTERNAL void
hash_table_destroy(struct hash_table *ht,
		   void (*delete_function)(struct hash_entry *entry));

SQFS_INTERNAL struct hash_entry *
hash_table_insert_pre_hashed(struct hash_table *ht, sqfs_u32 hash,
                             const void *key, void *data);

SQFS_INTERNAL struct hash_entry *
hash_table_search_pre_hashed(struct hash_table *ht, sqfs_u32 hash,
                             const void *key);

SQFS_INTERNAL struct hash_entry *hash_table_next_entry(struct hash_table *ht,
						       struct hash_entry *entry);

/**
 * This foreach function is safe against deletion (which just replaces
 * an entry's data with the deleted marker), but not against insertion
 * (which may rehash the table, making entry a dangling pointer).
 */
#define hash_table_foreach(ht, entry)                                      \
   for (struct hash_entry *entry = hash_table_next_entry(ht, NULL);  \
        entry != NULL;                                                     \
        entry = hash_table_next_entry(ht, entry))

#endif /* _HASH_TABLE_H */
