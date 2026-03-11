/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XATTR_WRITER_H
#define XATTR_WRITER_H

#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/meta_writer.h"
#include "sqfs/super.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"

#include "util/str_table.h"
#include "util/rbtree.h"
#include "util/array.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define XATTR_INITIAL_PAIR_CAP 128

#define MK_PAIR(key, value) (((sqfs_u64)(key) << 32UL) | (sqfs_u64)(value))
#define GET_KEY(pair) ((pair >> 32UL) & 0x0FFFFFFFFUL)
#define GET_VALUE(pair) (pair & 0x0FFFFFFFFUL)


typedef struct kv_block_desc_t {
	struct kv_block_desc_t *next;
	size_t start;
	size_t count;

	sqfs_u64 start_ref;
	size_t size_bytes;
} kv_block_desc_t;

struct sqfs_xattr_writer_t {
	sqfs_object_t base;

	str_table_t keys;
	str_table_t values;

	array_t kv_pairs;

	size_t kv_start;

	rbtree_t kv_block_tree;
	kv_block_desc_t *kv_block_first;
	kv_block_desc_t *kv_block_last;
	size_t num_blocks;
};

#endif /* XATTR_WRITER_H */
