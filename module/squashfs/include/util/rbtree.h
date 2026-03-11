/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * rbtree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_RBTREE_H
#define SQFS_RBTREE_H

#include "config.h"
#include "sqfs/predef.h"
#include "mempool.h"
#include "compat.h"

#include <stddef.h>

typedef struct rbtree_node_t {
	struct rbtree_node_t *left;
	struct rbtree_node_t *right;
	sqfs_u32 value_offset;
	sqfs_u32 is_red : 1;
	sqfs_u32 pad0 : 31;

	sqfs_u8 data[];
} rbtree_node_t;

typedef struct rbtree_t {
	rbtree_node_t *root;

#ifndef NO_CUSTOM_ALLOC
	mem_pool_t *pool;
#endif

	int (*key_compare)(const void *ctx,
			   const void *lhs, const void *hrs);

	size_t key_size;
	size_t key_size_padded;

	size_t value_size;

	void *key_context;
} rbtree_t;

static SQFS_INLINE void *rbtree_node_key(rbtree_node_t *n)
{
	return n->data;
}

static SQFS_INLINE void *rbtree_node_value(rbtree_node_t *n)
{
	return n->data + n->value_offset;
}

#ifdef __cplusplus
extern "C" {
#endif

SQFS_INTERNAL int rbtree_init(rbtree_t *tree, size_t keysize, size_t valuesize,
			      int(*key_compare)(const void *, const void *,
						const void *));

SQFS_INTERNAL void rbtree_cleanup(rbtree_t *tree);

SQFS_INTERNAL int rbtree_copy(const rbtree_t *tree, rbtree_t *out);

SQFS_INTERNAL int rbtree_insert(rbtree_t *tree, const void *key,
				const void *value);

SQFS_INTERNAL rbtree_node_t *rbtree_lookup(const rbtree_t *tree,
					   const void *key);

#ifdef __cplusplus
}
#endif

#endif /* TOOLS_RBTREE_H */

