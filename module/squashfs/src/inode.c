/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/dir_entry.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"

#include <string.h>
#include <stdlib.h>

#include "util/util.h"

static int inverse_type[] = {
	[SQFS_INODE_DIR] = SQFS_INODE_EXT_DIR,
	[SQFS_INODE_FILE] = SQFS_INODE_EXT_FILE,
	[SQFS_INODE_SLINK] = SQFS_INODE_EXT_SLINK,
	[SQFS_INODE_BDEV] = SQFS_INODE_EXT_BDEV,
	[SQFS_INODE_CDEV] = SQFS_INODE_EXT_CDEV,
	[SQFS_INODE_FIFO] = SQFS_INODE_EXT_FIFO,
	[SQFS_INODE_SOCKET] = SQFS_INODE_EXT_SOCKET,
	[SQFS_INODE_EXT_DIR] = SQFS_INODE_DIR,
	[SQFS_INODE_EXT_FILE] = SQFS_INODE_FILE,
	[SQFS_INODE_EXT_SLINK] = SQFS_INODE_SLINK,
	[SQFS_INODE_EXT_BDEV] = SQFS_INODE_BDEV,
	[SQFS_INODE_EXT_CDEV] = SQFS_INODE_CDEV,
	[SQFS_INODE_EXT_FIFO] = SQFS_INODE_FIFO,
	[SQFS_INODE_EXT_SOCKET] = SQFS_INODE_SOCKET,
};

int sqfs_inode_get_xattr_index(const sqfs_inode_generic_t *inode,
			       sqfs_u32 *out)
{
	switch (inode->base.type) {
	case SQFS_INODE_DIR:
	case SQFS_INODE_FILE:
	case SQFS_INODE_SLINK:
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		*out = 0xFFFFFFFF;
		break;
	case SQFS_INODE_EXT_DIR:
		*out = inode->data.dir_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FILE:
		*out = inode->data.file_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_SLINK:
		*out = inode->data.slink_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		*out = inode->data.dev_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		*out = inode->data.ipc_ext.xattr_idx;
		break;
	default:
		return SQFS_ERROR_CORRUPTED;
	}

	return 0;
}

int sqfs_inode_set_xattr_index(sqfs_inode_generic_t *inode, sqfs_u32 index)
{
	int err;

	if (index != 0xFFFFFFFF) {
		err = sqfs_inode_make_extended(inode);
		if (err)
			return err;
	}

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
	case SQFS_INODE_FILE:
	case SQFS_INODE_SLINK:
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		break;
	case SQFS_INODE_EXT_DIR:
		inode->data.dir_ext.xattr_idx = index;
		break;
	case SQFS_INODE_EXT_FILE:
		inode->data.file_ext.xattr_idx = index;
		break;
	case SQFS_INODE_EXT_SLINK:
		inode->data.slink_ext.xattr_idx = index;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		inode->data.dev_ext.xattr_idx = index;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		inode->data.ipc_ext.xattr_idx = index;
		break;
	default:
		return SQFS_ERROR_CORRUPTED;
	}

	return 0;
}

int sqfs_inode_make_extended(sqfs_inode_generic_t *inode)
{
	switch (inode->base.type) {
	case SQFS_INODE_DIR: {
		sqfs_inode_dir_ext_t temp = {
			.nlink = inode->data.dir.nlink,
			.size = inode->data.dir.size,
			.start_block = inode->data.dir.start_block,
			.parent_inode = inode->data.dir.parent_inode,
			.inodex_count = 0,
			.offset = inode->data.dir.offset,
			.xattr_idx = 0xFFFFFFFF,
		};
		inode->data.dir_ext = temp;
		break;
	}
	case SQFS_INODE_FILE: {
		sqfs_inode_file_ext_t temp = {
			.blocks_start = inode->data.file.blocks_start,
			.file_size = inode->data.file.file_size,
			.sparse = 0,
			.nlink = 1,
			.fragment_idx = inode->data.file.fragment_index,
			.fragment_offset = inode->data.file.fragment_offset,
			.xattr_idx = 0xFFFFFFFF,
		};
		inode->data.file_ext = temp;
		break;
	}
	case SQFS_INODE_SLINK:
		inode->data.slink_ext.xattr_idx = 0xFFFFFFFF;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		inode->data.dev_ext.xattr_idx = 0xFFFFFFFF;
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		inode->data.dev_ext.xattr_idx = 0xFFFFFFFF;
		break;
	case SQFS_INODE_EXT_DIR:
	case SQFS_INODE_EXT_FILE:
	case SQFS_INODE_EXT_SLINK:
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		return 0;
	default:
		return SQFS_ERROR_CORRUPTED;
	}

	inode->base.type = inverse_type[inode->base.type];
	return 0;
}

int sqfs_inode_make_basic(sqfs_inode_generic_t *inode)
{
	sqfs_u32 xattr;
	int err;

	err = sqfs_inode_get_xattr_index(inode, &xattr);
	if (err != 0 || xattr != 0xFFFFFFFF)
		return err;

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
	case SQFS_INODE_FILE:
	case SQFS_INODE_SLINK:
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		return 0;
	case SQFS_INODE_EXT_DIR: {
		sqfs_inode_dir_t temp = {
			.start_block = inode->data.dir_ext.start_block,
			.nlink = inode->data.dir_ext.nlink,
			.size = inode->data.dir_ext.size,
			.offset = inode->data.dir_ext.offset,
			.parent_inode = inode->data.dir_ext.parent_inode,
		};

		if (inode->data.dir_ext.size > 0x0FFFF)
			return 0;

		inode->data.dir = temp;
		break;
	}
	case SQFS_INODE_EXT_FILE: {
		sqfs_inode_file_t temp = {
			.blocks_start = inode->data.file_ext.blocks_start,
			.fragment_index = inode->data.file_ext.fragment_idx,
			.fragment_offset = inode->data.file_ext.fragment_offset,
			.file_size = inode->data.file_ext.file_size,
		};

		if (inode->data.file_ext.blocks_start > 0x0FFFFFFFFUL)
			return 0;
		if (inode->data.file_ext.file_size > 0x0FFFFFFFFUL)
			return 0;
		if (inode->data.file_ext.sparse > 0)
			return 0;
		if (inode->data.file_ext.nlink > 1)
			return 0;

		inode->data.file = temp;
		break;
	}
	case SQFS_INODE_EXT_SLINK:
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		break;
	default:
		return SQFS_ERROR_CORRUPTED;
	}

	inode->base.type = inverse_type[inode->base.type];
	return 0;
}

int sqfs_inode_set_file_size(sqfs_inode_generic_t *inode, sqfs_u64 size)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		inode->data.file_ext.file_size = size;

		if (size < 0x0FFFFFFFFUL)
			sqfs_inode_make_basic(inode);
	} else if (inode->base.type == SQFS_INODE_FILE) {
		if (size > 0x0FFFFFFFFUL) {
			sqfs_inode_make_extended(inode);
			inode->data.file_ext.file_size = size;
		} else {
			inode->data.file.file_size = size;
		}
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_set_frag_location(sqfs_inode_generic_t *inode,
				 sqfs_u32 index, sqfs_u32 offset)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		inode->data.file_ext.fragment_idx = index;
		inode->data.file_ext.fragment_offset = offset;
	} else if (inode->base.type == SQFS_INODE_FILE) {
		inode->data.file.fragment_index = index;
		inode->data.file.fragment_offset = offset;
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_set_file_block_start(sqfs_inode_generic_t *inode,
				    sqfs_u64 location)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		inode->data.file_ext.blocks_start = location;

		if (location < 0x0FFFFFFFFUL)
			sqfs_inode_make_basic(inode);
	} else if (inode->base.type == SQFS_INODE_FILE) {
		if (location > 0x0FFFFFFFFUL) {
			sqfs_inode_make_extended(inode);
			inode->data.file_ext.blocks_start = location;
		} else {
			inode->data.file.blocks_start = location;
		}
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_get_file_size(const sqfs_inode_generic_t *inode, sqfs_u64 *size)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		*size = inode->data.file_ext.file_size;
	} else if (inode->base.type == SQFS_INODE_FILE) {
		*size = inode->data.file.file_size;
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_get_frag_location(const sqfs_inode_generic_t *inode,
				 sqfs_u32 *index, sqfs_u32 *offset)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		*index = inode->data.file_ext.fragment_idx;
		*offset = inode->data.file_ext.fragment_offset;
	} else if (inode->base.type == SQFS_INODE_FILE) {
		*index = inode->data.file.fragment_index;
		*offset = inode->data.file.fragment_offset;
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_get_file_block_start(const sqfs_inode_generic_t *inode,
				    sqfs_u64 *location)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		*location = inode->data.file_ext.blocks_start;
	} else if (inode->base.type == SQFS_INODE_FILE) {
		*location = inode->data.file.blocks_start;
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	return 0;
}

int sqfs_inode_unpack_dir_index_entry(const sqfs_inode_generic_t *inode,
				      sqfs_dir_index_t **out,
				      size_t index)
{
	sqfs_dir_index_t ent;
	const char *ptr;
	size_t offset;

	if (inode->base.type != SQFS_INODE_EXT_DIR) {
		if (inode->base.type == SQFS_INODE_DIR)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		return SQFS_ERROR_NOT_DIR;
	}

	offset = 0;
	ptr = (const char *)inode->extra;

	for (;;) {
		if (offset >= inode->payload_bytes_used)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		if (index == 0)
			break;

		memcpy(&ent, ptr + offset, sizeof(ent));
		offset += sizeof(ent) + ent.size + 1;
		index -= 1;
	}

	memcpy(&ent, ptr + offset, sizeof(ent));

	*out = alloc_flex(sizeof(ent), 1, ent.size + 2);
	if (*out == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(*out, &ent, sizeof(ent));
	memcpy((*out)->name, ptr + offset + sizeof(ent), ent.size + 1);
	return 0;
}

int sqfs_dir_entry_from_inode(const char *name, size_t len,
			      const sqfs_inode_generic_t *inode,
			      const sqfs_id_table_t *idtbl,
			      sqfs_dir_entry_t **out)
{
	sqfs_dir_entry_t *ent;
	sqfs_u32 uid, gid;

	if (sqfs_id_table_index_to_id(idtbl, inode->base.uid_idx, &uid))
		return SQFS_ERROR_CORRUPTED;

	if (sqfs_id_table_index_to_id(idtbl, inode->base.gid_idx, &gid))
		return SQFS_ERROR_CORRUPTED;

	len = len > 0 ? strnlen(name, len) : strlen(name);
	ent = alloc_flex(sizeof(*ent), 1, len + 1);
	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(ent->name, name, len);
	ent->mode = inode->base.mode;
	ent->mtime = inode->base.mod_time;
	ent->uid = uid;
	ent->gid = gid;

	switch (inode->base.type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		ent->rdev = inode->data.dev.devno;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		ent->rdev = inode->data.dev_ext.devno;
		break;
	case SQFS_INODE_FILE:
		ent->size = inode->data.file.file_size;
		break;
	case SQFS_INODE_EXT_FILE:
		ent->size = inode->data.file_ext.file_size;
		break;
	case SQFS_INODE_DIR:
		ent->size = inode->data.dir.size;
		break;
	case SQFS_INODE_EXT_DIR:
		ent->size = inode->data.dir_ext.size;
		break;
	case SQFS_INODE_SLINK:
		ent->size = inode->data.slink.target_size;
		break;
	case SQFS_INODE_EXT_SLINK:
		ent->size = inode->data.slink_ext.target_size;
		break;
	default:
		break;
	}

	*out = ent;
	return 0;
}
