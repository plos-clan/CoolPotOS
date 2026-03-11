/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * data_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/data_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/frag_table.h"
#include "sqfs/block.h"
#include "sqfs/error.h"
#include "sqfs/table.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>

struct sqfs_data_reader_t {
	sqfs_object_t obj;

	sqfs_frag_table_t *frag_tbl;
	sqfs_compressor_t *cmp;
	sqfs_file_t *file;

	sqfs_u8 *data_block;
	size_t data_blk_size;
	sqfs_u64 current_block;

	sqfs_u8 *frag_block;
	size_t frag_blk_size;
	sqfs_u32 current_frag_index;
	sqfs_u32 block_size;

	sqfs_u8 scratch[];
};

static int get_block(sqfs_data_reader_t *data, sqfs_u64 off, sqfs_u32 size,
		     sqfs_u32 max_size, size_t *out_sz, sqfs_u8 **out)
{
	sqfs_u32 on_disk_size;
	sqfs_s32 ret;
	int err;

	*out = alloc_array(1, max_size);
	*out_sz = max_size;

	if (*out == NULL) {
		err = SQFS_ERROR_ALLOC;
		goto fail;
	}

	if (SQFS_IS_SPARSE_BLOCK(size))
		return 0;

	on_disk_size = SQFS_ON_DISK_BLOCK_SIZE(size);

	if (on_disk_size > max_size) {
		err = SQFS_ERROR_OVERFLOW;
		goto fail;
	}

	if (SQFS_IS_BLOCK_COMPRESSED(size)) {
		err = data->file->read_at(data->file, off,
					  data->scratch, on_disk_size);
		if (err)
			goto fail;

		ret = data->cmp->do_block(data->cmp, data->scratch,
					  on_disk_size, *out, max_size);
		if (ret <= 0) {
			err = ret < 0 ? ret : SQFS_ERROR_OVERFLOW;
			goto fail;
		}

		*out_sz = ret;
	} else {
		err = data->file->read_at(data->file, off,
					  *out, on_disk_size);
		if (err)
			goto fail;

		*out_sz = on_disk_size;
	}

	return 0;
fail:
	free(*out);
	*out = NULL;
	*out_sz = 0;
	return err;
}

static int precache_data_block(sqfs_data_reader_t *data, sqfs_u64 location,
			       sqfs_u32 size)
{
	if (data->data_block != NULL && data->current_block == location)
		return 0;

	free(data->data_block);
	data->current_block = location;

	return get_block(data, location, size, data->block_size,
			 &data->data_blk_size, &data->data_block);
}

static int precache_fragment_block(sqfs_data_reader_t *data, size_t idx)
{
	sqfs_fragment_t ent;
	int ret;

	if (data->frag_block != NULL && idx == data->current_frag_index)
		return 0;

	ret = sqfs_frag_table_lookup(data->frag_tbl, idx, &ent);
	if (ret != 0)
		return ret;

	free(data->frag_block);
	data->current_frag_index = idx;

	return get_block(data, ent.start_offset, ent.size, data->block_size,
			 &data->frag_blk_size, &data->frag_block);
}

static void data_reader_destroy(sqfs_object_t *obj)
{
	sqfs_data_reader_t *data = (sqfs_data_reader_t *)obj;

	sqfs_drop(data->cmp);
	sqfs_drop(data->file);
	sqfs_drop(data->frag_tbl);
	free(data->data_block);
	free(data->frag_block);
	free(data);
}

static sqfs_object_t *data_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_data_reader_t *data = (const sqfs_data_reader_t *)obj;
	sqfs_data_reader_t *copy;

	copy = alloc_flex(sizeof(*data), 1, data->block_size);
	if (copy == NULL)
		return NULL;

	memcpy(copy, data, sizeof(*data) + data->block_size);

	copy->frag_tbl = sqfs_copy(data->frag_tbl);
	if (copy->frag_tbl == NULL)
		goto fail_ftbl;

	if (data->data_block != NULL) {
		copy->data_block = malloc(data->data_blk_size);
		if (copy->data_block == NULL)
			goto fail_dblk;

		memcpy(copy->data_block, data->data_block,
		       data->data_blk_size);
	}

	if (copy->frag_block != NULL) {
		copy->frag_block = malloc(copy->frag_blk_size);
		if (copy->frag_block == NULL)
			goto fail_fblk;

		memcpy(copy->frag_block, data->frag_block,
		       data->frag_blk_size);
	}

	/* duplicate references */
	copy->file = sqfs_grab(copy->file);
	copy->cmp = sqfs_grab(copy->cmp);
	return (sqfs_object_t *)copy;
fail_fblk:
	free(copy->data_block);
fail_dblk:
	sqfs_drop(copy->frag_tbl);
fail_ftbl:
	free(copy);
	return NULL;
}

sqfs_data_reader_t *sqfs_data_reader_create(sqfs_file_t *file,
					    size_t block_size,
					    sqfs_compressor_t *cmp,
					    sqfs_u32 flags)
{
	sqfs_data_reader_t *data;

	if (flags != 0)
		return NULL;

	data = alloc_flex(sizeof(*data), 1, block_size);
	if (data == NULL)
		return NULL;

	sqfs_object_init(data, data_reader_destroy, data_reader_copy);

	data->frag_tbl = sqfs_frag_table_create(0);
	if (data->frag_tbl == NULL) {
		free(data);
		return NULL;
	}

	data->file = sqfs_grab(file);
	data->block_size = block_size;
	data->cmp = sqfs_grab(cmp);
	return data;
}

int sqfs_data_reader_load_fragment_table(sqfs_data_reader_t *data,
					 const sqfs_super_t *super)
{
	int ret;

	free(data->frag_block);
	data->frag_block = NULL;
	data->current_frag_index = 0;

	ret = sqfs_frag_table_read(data->frag_tbl, data->file,
				   super, data->cmp);
	if (ret != 0)
		return ret;

	data->current_frag_index = sqfs_frag_table_get_size(data->frag_tbl);
	return 0;
}

int sqfs_data_reader_get_block(sqfs_data_reader_t *data,
			       const sqfs_inode_generic_t *inode,
			       size_t index, size_t *size, sqfs_u8 **out)
{
	size_t i, unpacked_size;
	sqfs_u64 off, filesz;

	sqfs_inode_get_file_block_start(inode, &off);
	sqfs_inode_get_file_size(inode, &filesz);

	if (index >= sqfs_inode_get_file_block_count(inode))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	for (i = 0; i < index; ++i) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		filesz -= data->block_size;
	}

	unpacked_size = filesz < data->block_size ? filesz : data->block_size;

	return get_block(data, off, inode->extra[index],
			 unpacked_size, size, out);
}

int sqfs_data_reader_get_fragment(sqfs_data_reader_t *data,
				  const sqfs_inode_generic_t *inode,
				  size_t *size, sqfs_u8 **out)
{
	sqfs_u32 frag_idx, frag_off, frag_sz;
	size_t block_count;
	sqfs_u64 filesz;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	*size = 0;
	*out = NULL;

	block_count = sqfs_inode_get_file_block_count(inode);

	if (block_count > (UINT64_MAX / data->block_size))
		return SQFS_ERROR_OVERFLOW;

	if ((sqfs_u64)block_count * data->block_size >= filesz)
		return 0;

	frag_sz = filesz % data->block_size;

	err = precache_fragment_block(data, frag_idx);
	if (err)
		return err;

	if (frag_off + frag_sz > data->block_size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	*out = alloc_array(1, frag_sz);
	if (*out == NULL)
		return SQFS_ERROR_ALLOC;

	*size = frag_sz;
	memcpy(*out, (char *)data->frag_block + frag_off, frag_sz);
	return 0;
}

sqfs_s32 sqfs_data_reader_read(sqfs_data_reader_t *data,
			       const sqfs_inode_generic_t *inode,
			       sqfs_u64 offset, void *buffer, sqfs_u32 size)
{
	sqfs_u32 frag_idx, frag_off, diff, total = 0;
	size_t i, block_count;
	sqfs_u64 off, filesz;
	char *ptr;
	int err;

	if (size >= 0x7FFFFFFF)
		size = 0x7FFFFFFE;

	/* work out file location and size */
	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	sqfs_inode_get_file_block_start(inode, &off);
	block_count = sqfs_inode_get_file_block_count(inode);

	if (offset >= filesz)
		return 0;

	if ((filesz - offset) < (sqfs_u64)size)
		size = filesz - offset;

	if (size == 0)
		return 0;

	/* find location of the first block */
	for (i = 0; offset > data->block_size && i < block_count; ++i) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		offset -= data->block_size;
	}

	/* copy data from blocks */
	while (i < block_count && size > 0) {
		diff = data->block_size - offset;
		if (size < diff)
			diff = size;

		if (SQFS_IS_SPARSE_BLOCK(inode->extra[i])) {
			memset(buffer, 0, diff);
		} else {
			err = precache_data_block(data, off, inode->extra[i]);
			if (err)
				return err;

			memcpy(buffer, (char *)data->data_block + offset, diff);
			off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		}

		++i;
		offset = 0;
		size -= diff;
		total += diff;
		buffer = (char *)buffer + diff;
	}

	/* copy from fragment */
	if (size > 0) {
		err = precache_fragment_block(data, frag_idx);
		if (err)
			return err;

		if ((frag_off + offset) >= data->frag_blk_size)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		if ((data->frag_blk_size - (frag_off + offset)) < size)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		ptr = (char *)data->frag_block + frag_off + offset;
		memcpy(buffer, ptr, size);
		total += size;
	}

	return total;
}

/*****************************************************************************/

typedef struct {
	sqfs_istream_t base;

	sqfs_data_reader_t *rd;
	const char *filename;
	const sqfs_u32 *blocks;

	sqfs_u8 *buffer;
	size_t buf_used;
	size_t buf_off;

	sqfs_u64 filesz;
	sqfs_u64 disk_offset;

	sqfs_u32 frag_idx;
	sqfs_u32 frag_off;

	sqfs_u32 blk_idx;
	sqfs_u32 blk_count;

	sqfs_u32 inodata[];
} data_reader_istream_t;

static int dr_stream_get_buffered_data(sqfs_istream_t *base,
				       const sqfs_u8 **out,
				       size_t *size, size_t want)
{
	data_reader_istream_t *stream = (data_reader_istream_t *)base;
	sqfs_data_reader_t *rd = stream->rd;
	int ret;
	(void)want;

	if (stream->buf_off < stream->buf_used) {
		*out = stream->buffer + stream->buf_off;
		*size = stream->buf_used - stream->buf_off;
		return 0;
	}

	if (stream->filesz == 0) {
		ret = 1;
		goto fail;
	}

	stream->buf_off = 0;
	stream->buf_used = rd->block_size;
	if (stream->filesz < (sqfs_u64)stream->buf_used)
		stream->buf_used = stream->filesz;

	if (stream->blk_idx < stream->blk_count) {
		sqfs_u32 blkword = stream->blocks[stream->blk_idx++];
		sqfs_u32 disksz = SQFS_ON_DISK_BLOCK_SIZE(blkword);

		if (disksz == 0) {
			memset(stream->buffer, 0, stream->buf_used);
		} else if (SQFS_IS_BLOCK_COMPRESSED(blkword)) {
			ret = rd->file->read_at(rd->file, stream->disk_offset,
						rd->scratch, disksz);
			if (ret)
				goto fail;

			ret = rd->cmp->do_block(rd->cmp, rd->scratch, disksz,
						stream->buffer,
						stream->buf_used);
			if (ret <= 0) {
				ret = ret < 0 ? ret : SQFS_ERROR_OVERFLOW;
				goto fail;
			}

			if ((size_t)ret < stream->buf_used) {
				memset(stream->buffer + ret, 0,
				       stream->buf_used - ret);
			}
		} else {
			ret = rd->file->read_at(rd->file, stream->disk_offset,
						stream->buffer, disksz);
			if (ret)
				goto fail;
			if (disksz < stream->buf_used) {
				memset(stream->buffer + disksz, 0,
				       stream->buf_used - disksz);
			}
		}

		stream->disk_offset += disksz;
	} else {
		ret = precache_fragment_block(rd, stream->frag_idx);
		if (ret)
			return ret;

		if (rd->frag_blk_size < stream->frag_off ||
		    (rd->frag_blk_size - stream->frag_off) < stream->buf_used) {
			ret = SQFS_ERROR_CORRUPTED;
			goto fail;
		}

		memcpy(stream->buffer, rd->frag_block + stream->frag_off,
		       stream->buf_used);
	}

	stream->filesz -= stream->buf_used;
	*out = stream->buffer;
	*size = stream->buf_used;
	return 0;
fail:
	free(stream->buffer);
	stream->buffer = NULL;
	stream->buf_used = 0;
	stream->buf_off = 0;
	stream->filesz = 0;
	*out = NULL;
	*size = 0;
	return ret;
}

static void dr_stream_advance_buffer(sqfs_istream_t *base, size_t count)
{
	data_reader_istream_t *stream = (data_reader_istream_t *)base;
	size_t diff = stream->buf_used - stream->buf_off;

	stream->buf_off += (diff < count ? diff : count);
}

static const char *dr_stream_get_filename(sqfs_istream_t *base)
{
	return ((data_reader_istream_t *)base)->filename;
}

static void dr_stream_destroy(sqfs_object_t *obj)
{
	data_reader_istream_t *stream = (data_reader_istream_t *)obj;

	sqfs_drop(stream->rd);
	free(stream->buffer);
	free(stream);
}

int sqfs_data_reader_create_stream(sqfs_data_reader_t *data,
				   const sqfs_inode_generic_t *inode,
				   const char *filename, sqfs_istream_t **out)
{
	data_reader_istream_t *stream;
	size_t ino_sz, namelen, sz;
	sqfs_u64 filesz;
	char *nameptr;
	int ret;

	*out = NULL;

	ret = sqfs_inode_get_file_size(inode, &filesz);
	if (ret != 0)
		return ret;

	ino_sz = inode->payload_bytes_used;
	namelen = strlen(filename) + 1;

	if (SZ_ADD_OV(ino_sz, namelen, &sz))
		return SQFS_ERROR_ALLOC;
	if (SZ_ADD_OV(sz, sizeof(*stream), &sz))
		return SQFS_ERROR_ALLOC;

	stream = calloc(1, sz);
	if (stream == NULL)
		return SQFS_ERROR_ALLOC;

	stream->buffer = malloc(data->block_size);
	if (stream->buffer == NULL) {
		free(stream);
		return SQFS_ERROR_ALLOC;
	}

	sqfs_object_init(stream, dr_stream_destroy, NULL);

	memcpy(stream->inodata, inode->extra, ino_sz);
	stream->blocks = stream->inodata;
	stream->blk_count = ino_sz / sizeof(stream->blocks[0]);
	stream->filesz = filesz;

	nameptr = (char *)stream->inodata + ino_sz;
	memcpy(nameptr, filename, namelen);
	stream->filename = nameptr;

	sqfs_inode_get_file_block_start(inode, &stream->disk_offset);
	sqfs_inode_get_frag_location(inode, &stream->frag_idx,
				     &stream->frag_off);
	stream->rd = sqfs_grab(data);

	((sqfs_istream_t *)stream)->advance_buffer = dr_stream_advance_buffer;
	((sqfs_istream_t *)stream)->get_filename = dr_stream_get_filename;
	((sqfs_istream_t *)stream)->get_buffered_data =
		dr_stream_get_buffered_data;

	*out = (sqfs_istream_t *)stream;
	return 0;
}
