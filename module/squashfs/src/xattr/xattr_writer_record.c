/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer_record.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xattr_writer.h"

static const char *hexmap = "0123456789ABCDEF";

static char *to_base32(const void *input, size_t size)
{
	const sqfs_u8 *in = input;
	char *out, *ptr;
	size_t i;

	out = malloc(2 * size + 1);
	if (out == NULL)
		return NULL;

	ptr = out;

	for (i = 0; i < size; ++i) {
		*(ptr++) = hexmap[ in[i]       & 0x0F];
		*(ptr++) = hexmap[(in[i] >> 4) & 0x0F];
	}

	*ptr = '\0';
	return out;
}

static int compare_u64(const void *a, const void *b)
{
	sqfs_u64 lhs = *((const sqfs_u64 *)a);
	sqfs_u64 rhs = *((const sqfs_u64 *)b);

	return (lhs < rhs ? -1 : (lhs > rhs ? 1 : 0));
}

int sqfs_xattr_writer_begin(sqfs_xattr_writer_t *xwr, sqfs_u32 flags)
{
	if (flags != 0)
		return SQFS_ERROR_UNSUPPORTED;

	xwr->kv_start = xwr->kv_pairs.used;
	return 0;
}

int sqfs_xattr_writer_add_kv(sqfs_xattr_writer_t *xwr, const char *key,
			     const void *value, size_t size)
{
	size_t i, key_index, old_value_index, value_index;
	sqfs_u64 kv_pair;
	char *value_str;
	int err;

	if (sqfs_get_xattr_prefix_id(key) < 0)
		return SQFS_ERROR_UNSUPPORTED;

	err = str_table_get_index(&xwr->keys, key, &key_index);
	if (err)
		return err;

	value_str = to_base32(value, size);
	if (value_str == NULL)
		return SQFS_ERROR_ALLOC;

	err = str_table_get_index(&xwr->values, value_str, &value_index);
	free(value_str);
	if (err)
		return err;

	str_table_add_ref(&xwr->values, value_index);

	if (sizeof(size_t) > sizeof(sqfs_u32)) {
		if (key_index > 0x0FFFFFFFFUL || value_index > 0x0FFFFFFFFUL)
			return SQFS_ERROR_OVERFLOW;
	}

	kv_pair = MK_PAIR(key_index, value_index);

	for (i = xwr->kv_start; i < xwr->kv_pairs.used; ++i) {
		sqfs_u64 ent = ((sqfs_u64 *)xwr->kv_pairs.data)[i];

		if (ent == kv_pair)
			return 0;

		if (GET_KEY(ent) == key_index) {
			old_value_index = GET_VALUE(ent);

			str_table_del_ref(&xwr->values, old_value_index);

			((sqfs_u64 *)xwr->kv_pairs.data)[i] = kv_pair;
			return 0;
		}
	}

	return array_append(&xwr->kv_pairs, &kv_pair);
}

int sqfs_xattr_writer_add(sqfs_xattr_writer_t *xwr, const sqfs_xattr_t *ent)
{
	return sqfs_xattr_writer_add_kv(xwr, ent->key,
					ent->value, ent->value_len);
}

int sqfs_xattr_writer_end(sqfs_xattr_writer_t *xwr, sqfs_u32 *out)
{
	kv_block_desc_t blk;
	rbtree_node_t *n;
	sqfs_u32 index;
	int ret;

	memset(&blk, 0, sizeof(blk));
	blk.start = xwr->kv_start;
	blk.count = xwr->kv_pairs.used - xwr->kv_start;

	if (blk.count == 0) {
		*out = 0xFFFFFFFF;
		return 0;
	}

	array_sort_range(&xwr->kv_pairs, blk.start, blk.count, compare_u64);

	n = rbtree_lookup(&xwr->kv_block_tree, &blk);

	if (n != NULL) {
		index = *((sqfs_u32 *)rbtree_node_value(n));
		xwr->kv_pairs.used = xwr->kv_start;
	} else {
		index = xwr->num_blocks;

		ret = rbtree_insert(&xwr->kv_block_tree, &blk, &index);
		if (ret != 0)
			return ret;

		xwr->num_blocks += 1;
		n = rbtree_lookup(&xwr->kv_block_tree, &blk);

		if (xwr->kv_block_last == NULL) {
			xwr->kv_block_first = rbtree_node_key(n);
			xwr->kv_block_last = xwr->kv_block_first;
		} else {
			xwr->kv_block_last->next = rbtree_node_key(n);
			xwr->kv_block_last = xwr->kv_block_last->next;
		}
	}

	*out = index;
	return 0;
}
