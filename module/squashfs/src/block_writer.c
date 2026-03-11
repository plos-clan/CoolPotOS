/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/block_writer.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"

#include "util/array.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>

#define MK_BLK_HASH(chksum, size) \
	(((sqfs_u64)(size) << 32) | (sqfs_u64)(chksum))

#define SIZE_FROM_HASH(hash) ((hash >> 32) & ((1 << 24) - 1))

#define INIT_BLOCK_COUNT (128)
#define SCRATCH_SIZE (8192)

typedef struct {
	sqfs_u64 offset;
	sqfs_u64 hash;
} blk_info_t;

typedef struct {
	sqfs_block_writer_t base;
	sqfs_file_t *file;

	array_t blocks;

	size_t file_start;

	sqfs_u32 flags;

	sqfs_u8 scratch[];
} block_writer_default_t;

static int store_block_location(block_writer_default_t *wr, sqfs_u64 offset,
				sqfs_u32 size, sqfs_u32 chksum)
{
	blk_info_t info = { offset, MK_BLK_HASH(chksum, size) };

	return array_append(&(wr->blocks), &info);
}

static int deduplicate_blocks(block_writer_default_t *wr, sqfs_u32 flags, sqfs_u64 *out)
{
	const blk_info_t *blocks = wr->blocks.data;
	sqfs_u64 loc_a, loc_b, sz;
	size_t i, j, count;
	int ret;

	count = wr->blocks.used - wr->file_start;
	if (count == 0) {
		*out = 0;
		return 0;
	}

	if (flags & SQFS_BLK_DONT_DEDUPLICATE) {
		*out = blocks[wr->file_start].offset;
		return 0;
	}

	sz = 0;
	loc_a = blocks[wr->file_start].offset;

	for (i = 0; i < count; ++i)
		sz += SIZE_FROM_HASH(blocks[wr->file_start + i].hash);

	for (i = 0; i < wr->file_start; ++i) {
		for (j = 0; j < count; ++j) {
			if (blocks[i + j].hash !=
			    blocks[wr->file_start + j].hash)
				break;
		}

		if (j != count)
			continue;

		if (wr->flags & SQFS_BLOCK_WRITER_HASH_COMPARE_ONLY)
			break;

		loc_b = blocks[i].offset;

		ret = check_file_range_equal(wr->file, wr->scratch,
					     SCRATCH_SIZE, loc_a, loc_b, sz);
		if (ret == 0)
			break;
		if (ret < 0)
			return ret;
	}

	*out = blocks[i].offset;
	if (i >= wr->file_start)
		return 0;

	if (count >= (wr->file_start - i)) {
		wr->blocks.used = i + count;
	} else {
		wr->blocks.used = wr->file_start;
	}

	sz = blocks[wr->blocks.used - 1].offset +
		SIZE_FROM_HASH(blocks[wr->blocks.used - 1].hash);

	return wr->file->truncate(wr->file, sz);
}

static void block_writer_destroy(sqfs_object_t *wr)
{
	sqfs_drop(((block_writer_default_t *)wr)->file);
	array_cleanup(&(((block_writer_default_t *)wr)->blocks));
	free(wr);
}

static int write_data_block(sqfs_block_writer_t *base, void *user,
			    sqfs_u32 size, sqfs_u32 checksum, sqfs_u32 flags,
			    const sqfs_u8 *data, sqfs_u64 *location)
{
	block_writer_default_t *wr = (block_writer_default_t *)base;
	int err;
	(void)user;

	if (flags & SQFS_BLK_FIRST_BLOCK)
		wr->file_start = wr->blocks.used;

	*location = wr->file->get_size(wr->file);

	if (size != 0 && !(flags & SQFS_BLK_IS_SPARSE)) {
		sqfs_u32 out = size;
		if (!(flags & SQFS_BLK_IS_COMPRESSED))
			out |= 1 << 24;

		err = store_block_location(wr, *location, out, checksum);
		if (err)
			return err;

		err = wr->file->write_at(wr->file, *location, data, size);
		if (err)
			return err;
	}

	if (flags & SQFS_BLK_LAST_BLOCK)
		return deduplicate_blocks(wr, flags, location);

	return 0;
}

static sqfs_u64 get_block_count(const sqfs_block_writer_t *wr)
{
	return ((const block_writer_default_t *)wr)->blocks.used;
}

sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file, sqfs_u32 flags)
{
	block_writer_default_t *wr;

	if (flags & ~SQFS_BLOCK_WRITER_ALL_FLAGS)
		return NULL;

	if (flags & SQFS_BLOCK_WRITER_HASH_COMPARE_ONLY) {
		wr = calloc(1, sizeof(*wr));
	} else {
		wr = alloc_flex(sizeof(*wr), 1, SCRATCH_SIZE);
	}

	if (wr == NULL)
		return NULL;

	sqfs_object_init(wr, block_writer_destroy, NULL);

	((sqfs_block_writer_t *)wr)->write_data_block = write_data_block;
	((sqfs_block_writer_t *)wr)->get_block_count = get_block_count;
	wr->flags = flags;
	wr->file = sqfs_grab(file);

	if (array_init(&(wr->blocks), sizeof(blk_info_t), INIT_BLOCK_COUNT)) {
		sqfs_drop(wr->file);
		free(wr);
		return NULL;
	}

	return (sqfs_block_writer_t *)wr;
}
