/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xattr_writer.h"

static sqfs_object_t *xattr_writer_copy(const sqfs_object_t *obj)
{
	const sqfs_xattr_writer_t *xwr = (const sqfs_xattr_writer_t *)obj;
	sqfs_xattr_writer_t *copy;
	kv_block_desc_t *it;

	copy = calloc(1, sizeof(*copy));
	if (copy == NULL)
		return NULL;

	memcpy(copy, xwr, sizeof(*xwr));

	if (str_table_copy(&copy->keys, &xwr->keys))
		goto fail_keys;

	if (str_table_copy(&copy->values, &xwr->values))
		goto fail_values;

	if (array_init_copy(&copy->kv_pairs, &xwr->kv_pairs))
		goto fail_pairs;

	if (rbtree_copy(&xwr->kv_block_tree, &copy->kv_block_tree) != 0)
		goto fail_tree;

	for (it = xwr->kv_block_first; it != NULL; it = it->next) {
		rbtree_node_t *n = rbtree_lookup(&copy->kv_block_tree, it);

		if (copy->kv_block_last == NULL) {
			copy->kv_block_first = rbtree_node_key(n);
			copy->kv_block_last = copy->kv_block_first;
		} else {
			copy->kv_block_last->next = rbtree_node_key(n);
			copy->kv_block_last = copy->kv_block_last->next;
		}

		copy->kv_block_last->next = NULL;
	}

	return (sqfs_object_t *)copy;
fail_tree:
	array_cleanup(&copy->kv_pairs);
fail_pairs:
	str_table_cleanup(&copy->values);
fail_values:
	str_table_cleanup(&copy->keys);
fail_keys:
	free(copy);
	return NULL;
}

static void xattr_writer_destroy(sqfs_object_t *obj)
{
	sqfs_xattr_writer_t *xwr = (sqfs_xattr_writer_t *)obj;

	rbtree_cleanup(&xwr->kv_block_tree);
	array_cleanup(&xwr->kv_pairs);
	str_table_cleanup(&xwr->values);
	str_table_cleanup(&xwr->keys);
	free(xwr);
}

static int block_compare(const void *context,
			 const void *lhs, const void *rhs)
{
	const sqfs_xattr_writer_t *xwr = context;
	const kv_block_desc_t *l = lhs, *r = rhs;

	if (l->count != r->count)
		return l->count < r->count ? -1 : 1;

	if (l->start == r->start)
		return 0;

	return memcmp((sqfs_u64 *)xwr->kv_pairs.data + l->start,
		      (sqfs_u64 *)xwr->kv_pairs.data + r->start,
		      l->count * xwr->kv_pairs.size);
}

sqfs_xattr_writer_t *sqfs_xattr_writer_create(sqfs_u32 flags)
{
	sqfs_xattr_writer_t *xwr;

	if (flags != 0)
		return NULL;

	xwr = calloc(1, sizeof(*xwr));
	if (xwr == NULL)
		return NULL;

	sqfs_object_init(xwr, xattr_writer_destroy, xattr_writer_copy);

	if (str_table_init(&xwr->keys))
		goto fail_keys;

	if (str_table_init(&xwr->values))
		goto fail_values;

	if (array_init(&xwr->kv_pairs, sizeof(sqfs_u64),
		       XATTR_INITIAL_PAIR_CAP)) {
		goto fail_pairs;
	}

	if (rbtree_init(&xwr->kv_block_tree, sizeof(kv_block_desc_t),
			sizeof(sqfs_u32), block_compare)) {
		goto fail_tree;
	}

	xwr->kv_block_tree.key_context = xwr;
	return xwr;
fail_tree:
	array_cleanup(&xwr->kv_pairs);
fail_pairs:
	str_table_cleanup(&xwr->values);
fail_values:
	str_table_cleanup(&xwr->keys);
fail_keys:
	free(xwr);
	return NULL;
}
