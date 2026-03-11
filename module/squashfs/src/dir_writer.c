/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/dir_writer.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/dir.h"
#include "util/array.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>

#define DIR_INDEX_THRESHOLD (256)

typedef struct sqfs_dir_entry_t {
	struct sqfs_dir_entry_t *next;
	sqfs_u64 inode_ref;
	sqfs_u32 inode_num;
	sqfs_u16 type;
	size_t name_len;
	char name[];
} sqfs_dir_entry_t;

typedef struct index_ent_t {
	struct index_ent_t *next;
	sqfs_dir_entry_t *ent;
	sqfs_u64 block;
	sqfs_u32 index;
} index_ent_t;

struct sqfs_dir_writer_t {
	sqfs_object_t base;

	sqfs_dir_entry_t *list;
	sqfs_dir_entry_t *list_end;

	index_ent_t *idx;
	index_ent_t *idx_end;

	sqfs_u64 dir_ref;
	size_t dir_size;
	size_t ent_count;
	sqfs_meta_writer_t *dm;

	array_t export_tbl;
};

static int get_type(sqfs_u16 mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK: return SQFS_INODE_SOCKET;
	case S_IFIFO:  return SQFS_INODE_FIFO;
	case S_IFLNK:  return SQFS_INODE_SLINK;
	case S_IFBLK:  return SQFS_INODE_BDEV;
	case S_IFCHR:  return SQFS_INODE_CDEV;
	case S_IFDIR:  return SQFS_INODE_DIR;
	case S_IFREG:  return SQFS_INODE_FILE;
	default:
		break;
	}

	return SQFS_ERROR_UNSUPPORTED;
}

static void writer_reset(sqfs_dir_writer_t *writer)
{
	sqfs_dir_entry_t *ent;
	index_ent_t *idx;

	while (writer->idx != NULL) {
		idx = writer->idx;
		writer->idx = idx->next;
		free(idx);
	}

	while (writer->list != NULL) {
		ent = writer->list;
		writer->list = ent->next;
		free(ent);
	}

	writer->list_end = NULL;
	writer->idx_end = NULL;
	writer->dir_ref = 0;
	writer->dir_size = 0;
	writer->ent_count = 0;
}

static int add_export_table_entry(sqfs_dir_writer_t *writer,
				  sqfs_u32 inum, sqfs_u64 iref)
{
	sqfs_u64 *ptr;
	int ret;

	if (writer->export_tbl.data == NULL)
		return 0;

	if (inum < 1)
		return SQFS_ERROR_ARG_INVALID;

	ret = array_set_capacity(&writer->export_tbl, inum);
	if (ret != 0)
		return ret;

	ptr = (sqfs_u64 *)writer->export_tbl.data;

	if ((inum - 1) >= writer->export_tbl.used) {
		memset(ptr + writer->export_tbl.used, 0xFF,
		       (inum - writer->export_tbl.used) * sizeof(*ptr));

		writer->export_tbl.used = inum;
	}

	ptr[inum - 1] = iref;
	return 0;
}

static void dir_writer_destroy(sqfs_object_t *obj)
{
	sqfs_dir_writer_t *writer = (sqfs_dir_writer_t *)obj;

	sqfs_drop(writer->dm);
	writer_reset(writer);
	array_cleanup(&writer->export_tbl);
	free(writer);
}

sqfs_dir_writer_t *sqfs_dir_writer_create(sqfs_meta_writer_t *dm,
					  sqfs_u32 flags)
{
	sqfs_dir_writer_t *writer;

	if (flags & ~SQFS_DIR_WRITER_CREATE_ALL_FLAGS)
		return NULL;

	writer = calloc(1, sizeof(*writer));
	if (writer == NULL)
		return NULL;

	sqfs_object_init(writer, dir_writer_destroy, NULL);

	if (flags & SQFS_DIR_WRITER_CREATE_EXPORT_TABLE) {
		if (array_init(&writer->export_tbl, sizeof(sqfs_u64), 512)) {
			free(writer);
			return NULL;
		}

		memset(writer->export_tbl.data, 0xFF,
		       writer->export_tbl.size * writer->export_tbl.count);
	}

	writer->dm = sqfs_grab(dm);
	return writer;
}

int sqfs_dir_writer_begin(sqfs_dir_writer_t *writer, sqfs_u32 flags)
{
	sqfs_u32 offset;
	sqfs_u64 block;

	if (flags != 0)
		return SQFS_ERROR_UNSUPPORTED;

	writer_reset(writer);

	sqfs_meta_writer_get_position(writer->dm, &block, &offset);
	writer->dir_ref = (block << 16) | offset;
	return 0;
}

int sqfs_dir_writer_add_entry(sqfs_dir_writer_t *writer, const char *name,
			      sqfs_u32 inode_num, sqfs_u64 inode_ref,
			      sqfs_u16 mode)
{
	sqfs_dir_entry_t *ent;
	int type, err;

	type = get_type(mode);
	if (type < 0)
		return type;

	if (name[0] == '\0' || inode_num < 1)
		return SQFS_ERROR_ARG_INVALID;

	err = add_export_table_entry(writer, inode_num, inode_ref);
	if (err)
		return err;

	ent = alloc_flex(sizeof(*ent), 1, strlen(name));
	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	ent->inode_ref = inode_ref;
	ent->inode_num = inode_num;
	ent->type = type;
	ent->name_len = strlen(name);
	memcpy(ent->name, name, ent->name_len);

	if (writer->list_end == NULL) {
		writer->list = writer->list_end = ent;
	} else {
		writer->list_end->next = ent;
		writer->list_end = ent;
	}

	writer->ent_count += 1;
	return 0;
}

static size_t get_conseq_entry_count(sqfs_u32 offset, sqfs_dir_entry_t *head)
{
	size_t size, count = 0;
	sqfs_dir_entry_t *it;
	sqfs_s32 diff;

	size = (offset + sizeof(sqfs_dir_header_t)) % SQFS_META_BLOCK_SIZE;

	for (it = head; it != NULL; it = it->next) {
		if ((it->inode_ref >> 16) != (head->inode_ref >> 16))
			break;

		diff = it->inode_num - head->inode_num;

		if (diff > 32767 || diff < -32767)
			break;

		size += sizeof(sqfs_dir_node_t) + it->name_len;

		if (count > 0 && size > SQFS_META_BLOCK_SIZE)
			break;

		count += 1;

		if (count == SQFS_MAX_DIR_ENT)
			break;
	}

	return count;
}

static int add_header(sqfs_dir_writer_t *writer, size_t count,
		      sqfs_dir_entry_t *ref, sqfs_u64 block)
{
	sqfs_dir_header_t hdr;
	index_ent_t *idx;
	int err;

	hdr.count = htole32(count - 1);
	hdr.start_block = htole32(ref->inode_ref >> 16);
	hdr.inode_number = htole32(ref->inode_num);

	err = sqfs_meta_writer_append(writer->dm, &hdr, sizeof(hdr));
	if (err)
		return err;

	idx = calloc(1, sizeof(*idx));
	if (idx == NULL)
		return SQFS_ERROR_ALLOC;

	idx->ent = ref;
	idx->block = block;
	idx->index = writer->dir_size;

	if (writer->idx_end == NULL) {
		writer->idx = writer->idx_end = idx;
	} else {
		writer->idx_end->next = idx;
		writer->idx_end = idx;
	}

	writer->dir_size += sizeof(hdr);
	return 0;
}

int sqfs_dir_writer_end(sqfs_dir_writer_t *writer)
{
	sqfs_dir_entry_t *it, *first;
	sqfs_dir_node_t ent;
	sqfs_u16 *diff_u16;
	size_t i, count;
	sqfs_u32 offset;
	sqfs_u64 block;
	int err;

	for (it = writer->list; it != NULL; ) {
		sqfs_meta_writer_get_position(writer->dm, &block, &offset);
		count = get_conseq_entry_count(offset, it);

		err = add_header(writer, count, it, block);
		if (err)
			return err;

		first = it;

		for (i = 0; i < count; ++i) {
			ent.offset = htole16(it->inode_ref & 0x0000FFFF);
			ent.inode_diff = it->inode_num - first->inode_num;
			ent.type = htole16(it->type);
			ent.size = htole16(it->name_len - 1);

			diff_u16 = (sqfs_u16 *)&ent.inode_diff;
			*diff_u16 = htole16(*diff_u16);

			err = sqfs_meta_writer_append(writer->dm, &ent,
						      sizeof(ent));
			if (err)
				return err;

			err = sqfs_meta_writer_append(writer->dm, it->name,
						      it->name_len);
			if (err)
				return err;

			writer->dir_size += sizeof(ent) + it->name_len;
			it = it->next;
		}
	}

	return 0;
}

size_t sqfs_dir_writer_get_size(const sqfs_dir_writer_t *writer)
{
	return writer->dir_size;
}

sqfs_u64 sqfs_dir_writer_get_dir_reference(const sqfs_dir_writer_t *writer)
{
	return writer->dir_ref;
}

size_t sqfs_dir_writer_get_index_size(const sqfs_dir_writer_t *writer)
{
	size_t index_size = 0;
	index_ent_t *idx;

	for (idx = writer->idx; idx != NULL; idx = idx->next)
		index_size += sizeof(sqfs_dir_index_t) + idx->ent->name_len;

	return index_size;
}

size_t sqfs_dir_writer_get_entry_count(const sqfs_dir_writer_t *writer)
{
	return writer->ent_count;
}

sqfs_inode_generic_t
*sqfs_dir_writer_create_inode(const sqfs_dir_writer_t *writer,
			      size_t hlinks, sqfs_u32 xattr,
			      sqfs_u32 parent_ino)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_index_t ent;
	sqfs_u64 start_block;
	sqfs_u16 block_offset;
	size_t index_size;
	index_ent_t *idx;
	sqfs_u8 *ptr;

	index_size = 0;

	for (idx = writer->idx; idx != NULL; idx = idx->next)
		index_size += sizeof(ent) + idx->ent->name_len;

	inode = alloc_flex(sizeof(*inode), 1, index_size);
	if (inode == NULL)
		return NULL;

	inode->payload_bytes_available = index_size;
	start_block = writer->dir_ref >> 16;
	block_offset = writer->dir_ref & 0xFFFF;

	if (xattr != 0xFFFFFFFF || start_block > 0xFFFFFFFFUL ||
	    writer->dir_size > (0xFFFF - 3)) {
		inode->base.type = SQFS_INODE_EXT_DIR;
	} else {
		inode->base.type = SQFS_INODE_DIR;
	}

	if (writer->ent_count >= DIR_INDEX_THRESHOLD)
		inode->base.type = SQFS_INODE_EXT_DIR;

	if (inode->base.type == SQFS_INODE_DIR) {
		inode->data.dir.start_block = start_block;
		inode->data.dir.nlink = writer->ent_count + hlinks + 2;
		inode->data.dir.size = writer->dir_size + 3;
		inode->data.dir.offset = block_offset;
		inode->data.dir.parent_inode = parent_ino;
	} else {
		inode->data.dir_ext.nlink = writer->ent_count + hlinks + 2;
		inode->data.dir_ext.size = writer->dir_size + 3;
		inode->data.dir_ext.start_block = start_block;
		inode->data.dir_ext.parent_inode = parent_ino;
		inode->data.dir_ext.offset = block_offset;
		inode->data.dir_ext.xattr_idx = xattr;
		inode->data.dir_ext.inodex_count = 0;
		inode->payload_bytes_used = 0;

		for (idx = writer->idx; idx != NULL; idx = idx->next) {
			memset(&ent, 0, sizeof(ent));
			ent.start_block = idx->block;
			ent.index = idx->index;
			ent.size = idx->ent->name_len - 1;

			ptr = (sqfs_u8 *)inode->extra +
				inode->payload_bytes_used;
			memcpy(ptr, &ent, sizeof(ent));
			memcpy(ptr + sizeof(ent), idx->ent->name,
			       idx->ent->name_len);

			inode->data.dir_ext.inodex_count += 1;
			inode->payload_bytes_used += sizeof(ent);
			inode->payload_bytes_used += idx->ent->name_len;
		}
	}

	return inode;
}

int sqfs_dir_writer_write_export_table(sqfs_dir_writer_t *writer,
				       sqfs_file_t *file,
				       sqfs_compressor_t *cmp,
				       sqfs_u32 root_inode_num,
				       sqfs_u64 root_inode_ref,
				       sqfs_super_t *super)
{
	sqfs_u64 start;
	size_t size;
	int ret;

	ret = add_export_table_entry(writer, root_inode_num, root_inode_ref);
	if (ret)
		return 0;

	if (writer->export_tbl.data == NULL)
		return 0;

	size = writer->export_tbl.size * writer->export_tbl.used;

	ret = sqfs_write_table(file, cmp, writer->export_tbl.data,
			       size, &start);
	if (ret)
		return ret;

	super->export_table_start = start;
	super->flags |= SQFS_FLAG_EXPORTABLE;
	return 0;
}
