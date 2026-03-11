/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/block_writer.h"
#include "sqfs/frag_table.h"
#include "sqfs/compressor.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"

#include "util/hash_table.h"
#include "util/threadpool.h"
#include "util/util.h"

#include <string.h>
#include <stdlib.h>

typedef struct {
	sqfs_u32 index;
	sqfs_u32 offset;
	sqfs_u32 size;
	sqfs_u32 hash;
} chunk_info_t;

enum {
	BLK_FLAG_MANUAL_SUBMISSION = 0x10000000,
	BLK_FLAG_INTERNAL = 0x10000000,
};

typedef struct sqfs_block_t {
	struct sqfs_block_t *next;
	sqfs_inode_generic_t **inode;

	sqfs_u32 io_seq_num;
	sqfs_u32 flags;
	sqfs_u32 size;
	sqfs_u32 checksum;

	/* For data blocks: index within the inode.
	   For fragment fragment blocks: fragment table index. */
	sqfs_u32 index;

	/* User data pointer */
	void *user;

	sqfs_u8 data[];
} sqfs_block_t;

typedef struct worker_data_t {
	struct worker_data_t *next;
	sqfs_compressor_t *cmp;

	size_t scratch_size;
	sqfs_u8 scratch[];
} worker_data_t;

struct sqfs_block_processor_t {
	sqfs_object_t obj;

	sqfs_frag_table_t *frag_tbl;
	sqfs_block_t *frag_block;
	sqfs_block_writer_t *wr;

	sqfs_block_processor_stats_t stats;

	sqfs_inode_generic_t **inode;
	sqfs_block_t *blk_current;
	sqfs_u32 blk_flags;
	sqfs_u32 blk_index;
	void *user;

	struct hash_table *frag_ht;
	sqfs_block_t *free_list;

	size_t max_block_size;
	size_t max_backlog;
	size_t backlog;

	bool begin_called;

	sqfs_file_t *file;
	sqfs_compressor_t *uncmp;

	thread_pool_t *pool;
	worker_data_t *workers;

	sqfs_block_t *io_queue;
	sqfs_u32 io_seq_num;
	sqfs_u32 io_deq_seq_num;

	sqfs_block_t *current_frag;
	sqfs_block_t *cached_frag_blk;
	sqfs_block_t *fblk_in_flight;
	int fblk_lookup_error;

	sqfs_u8 scratch[];
};

SQFS_INTERNAL int enqueue_block(sqfs_block_processor_t *proc,
				sqfs_block_t *blk);

SQFS_INTERNAL int dequeue_block(sqfs_block_processor_t *proc);

#endif /* INTERNAL_H */
