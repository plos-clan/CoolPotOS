/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * meta_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>

struct sqfs_meta_reader_t {
	sqfs_object_t base;

	sqfs_u64 start;
	sqfs_u64 limit;
	size_t data_used;

	/* The location of the current block in the image */
	sqfs_u64 block_offset;

	/* The location of the next block after the current one */
	sqfs_u64 next_block;

	/* A byte offset into the uncompressed data of the current block */
	size_t offset;

	/* The underlying file descriptor to read from */
	sqfs_file_t *file;

	/* A pointer to the compressor to use for extracting data */
	sqfs_compressor_t *cmp;

	/* The raw data read from the input file */
	sqfs_u8 data[SQFS_META_BLOCK_SIZE];

	/* The uncompressed data read from the input file */
	sqfs_u8 scratch[SQFS_META_BLOCK_SIZE];
};

static void meta_reader_destroy(sqfs_object_t *m)
{
	sqfs_meta_reader_t *mr = (sqfs_meta_reader_t *)m;

	sqfs_drop(mr->file);
	sqfs_drop(mr->cmp);
	free(m);
}

static sqfs_object_t *meta_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_meta_reader_t *m = (const sqfs_meta_reader_t *)obj;
	sqfs_meta_reader_t *copy = malloc(sizeof(*copy));

	if (copy != NULL) {
		memcpy(copy, m, sizeof(*m));

		/* duplicate references */
		copy->cmp = sqfs_grab(copy->cmp);
		copy->file = sqfs_grab(copy->file);
	}

	return (sqfs_object_t *)copy;
}

sqfs_meta_reader_t *sqfs_meta_reader_create(sqfs_file_t *file,
					    sqfs_compressor_t *cmp,
					    sqfs_u64 start, sqfs_u64 limit)
{
	sqfs_meta_reader_t *m = calloc(1, sizeof(*m));

	if (m == NULL)
		return NULL;

	sqfs_object_init(m, meta_reader_destroy, meta_reader_copy);

	m->block_offset = 0xFFFFFFFFFFFFFFFFUL;
	m->start = start;
	m->limit = limit;
	m->file = sqfs_grab(file);
	m->cmp = sqfs_grab(cmp);
	return m;
}

int sqfs_meta_reader_seek(sqfs_meta_reader_t *m, sqfs_u64 block_start,
			  size_t offset)
{
	bool compressed;
	sqfs_u16 header;
	sqfs_u32 size;
	sqfs_s32 ret;
	int err;

	if (block_start < m->start || block_start >= m->limit)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (block_start == m->block_offset) {
		if (offset >= m->data_used)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		m->offset = offset;
		return 0;
	}

	err = m->file->read_at(m->file, block_start, &header, 2);
	if (err)
		return err;

	header = le16toh(header);
	compressed = (header & 0x8000) == 0;
	size = header & 0x7FFF;

	if (size > sizeof(m->data))
		return SQFS_ERROR_CORRUPTED;

	if ((block_start + 2 + size) > m->limit)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	err = m->file->read_at(m->file, block_start + 2, m->data, size);
	if (err)
		return err;

	if (compressed) {
		ret = m->cmp->do_block(m->cmp, m->data, size,
				       m->scratch, sizeof(m->scratch));

		if (ret < 0)
			return ret;

		memcpy(m->data, m->scratch, ret);
		m->data_used = ret;
	} else {
		m->data_used = size;
	}

	if (offset >= m->data_used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	m->block_offset = block_start;
	m->next_block = block_start + size + 2;
	m->offset = offset;
	return 0;
}

void sqfs_meta_reader_get_position(const sqfs_meta_reader_t *m,
				   sqfs_u64 *block_start, size_t *offset)
{
	if (m->offset == m->data_used) {
		*block_start = m->next_block;
		*offset = 0;
	} else {
		*block_start = m->block_offset;
		*offset = m->offset;
	}
}

int sqfs_meta_reader_read(sqfs_meta_reader_t *m, void *data, size_t size)
{
	size_t diff;
	int ret;

	while (size != 0) {
		diff = m->data_used - m->offset;

		if (diff == 0) {
			ret = sqfs_meta_reader_seek(m, m->next_block, 0);
			if (ret)
				return ret;
			diff = m->data_used;
		}

		if (diff > size)
			diff = size;

		memcpy(data, m->data + m->offset, diff);

		m->offset += diff;
		data = (char *)data + diff;
		size -= diff;
	}

	return 0;
}
