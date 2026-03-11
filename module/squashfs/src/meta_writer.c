/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * meta_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/compressor.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <string.h>
#include <stdlib.h>

typedef struct meta_block_t {
	struct meta_block_t *next;

	/* possibly compressed data with 2 byte header */
	sqfs_u8 data[SQFS_META_BLOCK_SIZE + 2];
} meta_block_t;

struct sqfs_meta_writer_t {
	sqfs_object_t base;

	/* A byte offset into the uncompressed data of the current block */
	size_t offset;

	/* The location of the current block in the file */
	size_t block_offset;

	/* The underlying file descriptor to write to */
	sqfs_file_t *file;

	/* A pointer to the compressor to use for compressing the data */
	sqfs_compressor_t *cmp;

	/* The raw data chunk that data is appended to */
	sqfs_u8 data[SQFS_META_BLOCK_SIZE];

	sqfs_u32 flags;
	meta_block_t *list;
	meta_block_t *list_end;
};

static int write_block(sqfs_file_t *file, meta_block_t *outblk)
{
	sqfs_u16 header;
	size_t count;
	sqfs_u64 off;

	memcpy(&header, outblk->data, sizeof(header));
	count = le16toh(header) & 0x7FFF;
	off = file->get_size(file);

	return file->write_at(file, off, outblk->data, count + 2);
}

static void meta_writer_destroy(sqfs_object_t *obj)
{
	sqfs_meta_writer_t *m = (sqfs_meta_writer_t *)obj;
	meta_block_t *blk;

	while (m->list != NULL) {
		blk = m->list;
		m->list = blk->next;
		free(blk);
	}

	sqfs_drop(m->file);
	sqfs_drop(m->cmp);
	free(m);
}

sqfs_meta_writer_t *sqfs_meta_writer_create(sqfs_file_t *file,
					    sqfs_compressor_t *cmp,
					    sqfs_u32 flags)
{
	sqfs_meta_writer_t *m;

	if (flags & ~SQFS_META_WRITER_ALL_FLAGS)
		return NULL;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		return NULL;

	sqfs_object_init(m, meta_writer_destroy, NULL);

	m->cmp = sqfs_grab(cmp);
	m->file = sqfs_grab(file);
	m->flags = flags;
	return m;
}

int sqfs_meta_writer_flush(sqfs_meta_writer_t *m)
{
	meta_block_t *outblk;
	sqfs_u16 header;
	sqfs_u32 count;
	sqfs_s32 ret;

	if (m->offset == 0)
		return 0;

	outblk = calloc(1, sizeof(*outblk));
	if (outblk == NULL)
		return SQFS_ERROR_ALLOC;

	ret = m->cmp->do_block(m->cmp, m->data, m->offset,
			       outblk->data + 2, sizeof(outblk->data) - 2);
	if (ret < 0) {
		free(outblk);
		return ret;
	}

	if (ret > 0) {
		header = htole16(ret);
		count = ret + 2;
	} else {
		header = htole16(m->offset | 0x8000);
		memcpy(outblk->data + 2, m->data, m->offset);
		count = m->offset + 2;
	}

	memcpy(outblk->data, &header, sizeof(header));

	ret = 0;

	if (m->flags & SQFS_META_WRITER_KEEP_IN_MEMORY) {
		if (m->list == NULL) {
			m->list = outblk;
		} else {
			m->list_end->next = outblk;
		}
		m->list_end = outblk;
	} else {
		ret = write_block(m->file, outblk);
		free(outblk);
	}

	memset(m->data, 0, sizeof(m->data));
	m->offset = 0;
	m->block_offset += count;
	return ret;
}

int sqfs_meta_writer_append(sqfs_meta_writer_t *m, const void *data,
			    size_t size)
{
	size_t diff;
	int ret;

	while (size != 0) {
		diff = sizeof(m->data) - m->offset;

		if (diff == 0) {
			ret = sqfs_meta_writer_flush(m);
			if (ret)
				return ret;
			diff = sizeof(m->data);
		}

		if (diff > size)
			diff = size;

		memcpy(m->data + m->offset, data, diff);
		m->offset += diff;
		size -= diff;
		data = (const char *)data + diff;
	}

	if (m->offset == sizeof(m->data))
		return sqfs_meta_writer_flush(m);

	return 0;
}

void sqfs_meta_writer_get_position(const sqfs_meta_writer_t *m,
				   sqfs_u64 *block_start,
				   sqfs_u32 *offset)
{
	*block_start = m->block_offset;
	*offset = m->offset;
}

void sqfs_meta_writer_reset(sqfs_meta_writer_t *m)
{
	m->block_offset = 0;
	m->offset = 0;
}

int sqfs_meta_write_write_to_file(sqfs_meta_writer_t *m)
{
	meta_block_t *blk;
	int ret;

	while (m->list != NULL) {
		blk = m->list;

		ret = write_block(m->file, blk);
		if (ret)
			return ret;

		m->list = blk->next;
		free(blk);
	}

	m->list_end = NULL;
	return 0;
}
