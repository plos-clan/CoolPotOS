/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * readdir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/error.h"
#include "sqfs/super.h"
#include "sqfs/inode.h"
#include "sqfs/dir.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
				     sqfs_dir_header_t *hdr)
{
	int err = sqfs_meta_reader_read(m, hdr, sizeof(*hdr));
	if (err)
		return err;

	hdr->count = le32toh(hdr->count);
	hdr->start_block = le32toh(hdr->start_block);
	hdr->inode_number = le32toh(hdr->inode_number);

	if (hdr->count > (SQFS_MAX_DIR_ENT - 1))
		return SQFS_ERROR_CORRUPTED;

	return 0;
}

int sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m,
				  sqfs_dir_node_t **result)
{
	sqfs_dir_node_t ent, *out;
	sqfs_u16 *diff_u16;
	int err;

	err = sqfs_meta_reader_read(m, &ent, sizeof(ent));
	if (err)
		return err;

	diff_u16 = (sqfs_u16 *)&ent.inode_diff;
	*diff_u16 = le16toh(*diff_u16);

	ent.offset = le16toh(ent.offset);
	ent.type = le16toh(ent.type);
	ent.size = le16toh(ent.size);

	out = calloc(1, sizeof(*out) + ent.size + 2);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = ent;
	err = sqfs_meta_reader_read(m, out->name, ent.size + 1);
	if (err) {
		free(out);
		return err;
	}

	*result = out;
	return 0;
}

int sqfs_readdir_state_init(sqfs_readdir_state_t *s, const sqfs_super_t *super,
			    const sqfs_inode_generic_t *inode)
{
	memset(s, 0, sizeof(*s));

	if (inode->base.type == SQFS_INODE_DIR) {
		s->block = inode->data.dir.start_block;
		s->offset = inode->data.dir.offset;
		s->size = inode->data.dir.size;
	} else if (inode->base.type == SQFS_INODE_EXT_DIR) {
		s->block = inode->data.dir_ext.start_block;
		s->offset = inode->data.dir_ext.offset;
		s->size = inode->data.dir_ext.size;
	} else {
		return SQFS_ERROR_NOT_DIR;
	}

	s->block += super->directory_table_start;
	return 0;
}

int sqfs_meta_reader_readdir(sqfs_meta_reader_t *m, sqfs_readdir_state_t *it,
			     sqfs_dir_node_t **ent,
			     sqfs_u32 *inum, sqfs_u64 *iref)
{
	size_t count;
	int ret;

	if (it->entries == 0) {
		sqfs_dir_header_t hdr;

		if (it->size <= sizeof(hdr))
			goto out_eof;

		ret = sqfs_meta_reader_seek(m, it->block, it->offset);
		if (ret != 0)
			return ret;

		ret = sqfs_meta_reader_read_dir_header(m, &hdr);
		if (ret != 0)
			return ret;

		sqfs_meta_reader_get_position(m, &it->block, &it->offset);

		it->size -= sizeof(hdr);
		it->entries = hdr.count + 1;
		it->inum_base = hdr.inode_number;
		it->inode_block = hdr.start_block;
	}

	if (it->size <= sizeof(**ent))
		goto out_eof;

	ret = sqfs_meta_reader_seek(m, it->block, it->offset);
	if (ret != 0)
		return ret;

	ret = sqfs_meta_reader_read_dir_ent(m, ent);
	if (ret)
		return ret;

	sqfs_meta_reader_get_position(m, &it->block, &it->offset);

	it->size -= sizeof(**ent);
	it->entries -= 1;

	count = (*ent)->size + 1;

	if (count >= it->size) {
		it->size = 0;
	} else {
		it->size -= count;
	}

	if (inum != NULL)
		*inum = it->inum_base + (*ent)->inode_diff;

	if (iref != NULL) {
		*iref = (sqfs_u64)it->inode_block << 16UL;
		*iref |= (*ent)->offset;
	}

	return 0;
out_eof:
	it->size = 0;
	it->entries = 0;
	return 1;
}
