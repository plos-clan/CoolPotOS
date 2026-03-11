/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "sqfs/dir_reader.h"
#include "sqfs/xattr_reader.h"
#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/inode.h"
#include "sqfs/id_table.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include "util/util.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	sqfs_dir_iterator_t base;

	sqfs_dir_reader_state_t state;

	sqfs_u32 xattr_idx;
	sqfs_inode_generic_t *inode;
	sqfs_dir_node_t *dent;

	sqfs_xattr_reader_t *xattr;
	sqfs_data_reader_t *data;
	sqfs_dir_reader_t *rd;
	sqfs_id_table_t *id;
} iterator_t;

static int it_next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	iterator_t *it = (iterator_t *)base;
	sqfs_dir_entry_t *ent = NULL;
	int ret;

	sqfs_free(it->inode);
	sqfs_free(it->dent);
	it->inode = NULL;
	it->dent = NULL;

	ret = sqfs_dir_reader_read(it->rd, &it->state, &it->dent);
	if (ret != 0)
		return ret;

	ret = sqfs_dir_reader_get_inode(it->rd, it->state.ent_ref, &it->inode);
	if (ret != 0)
		goto fail;

	ret = sqfs_dir_entry_from_inode((const char *)it->dent->name,
					it->dent->size + 1, it->inode,
					it->id, &ent);
	if (ret)
		goto fail;

	ent->inode = it->state.ent_ref;
	sqfs_inode_get_xattr_index(it->inode, &it->xattr_idx);

	*out = ent;
	return 0;
fail:
	sqfs_free(it->inode);
	sqfs_free(it->dent);
	sqfs_free(ent);
	it->inode = NULL;
	it->dent = NULL;
	return ret;
}

static int it_read_link(sqfs_dir_iterator_t *base, char **out)
{
	iterator_t *it = (iterator_t *)base;
	size_t size;

	*out = NULL;

	if (it->inode == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if (it->inode->base.type == SQFS_INODE_SLINK) {
		size = it->inode->data.slink.target_size;
	} else if (it->inode->base.type == SQFS_INODE_EXT_SLINK) {
		size = it->inode->data.slink.target_size;
	} else {
		return SQFS_ERROR_NO_ENTRY;
	}

	*out = calloc(1, size + 1);
	if (*out == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(*out, it->inode->extra, size);
	return 0;
}

static int it_open_subdir(sqfs_dir_iterator_t *base, sqfs_dir_iterator_t **out)
{
	iterator_t *it = (iterator_t *)base;

	*out = NULL;

	if (it->inode == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if (it->inode->base.type != SQFS_INODE_DIR &&
	    it->inode->base.type != SQFS_INODE_EXT_DIR) {
		return SQFS_ERROR_NOT_DIR;
	}

	return sqfs_dir_iterator_create(it->rd, it->id, it->data, it->xattr,
					it->inode, out);
}

static void it_ignore_subdir(sqfs_dir_iterator_t *it)
{
	(void)it;
}

static int it_open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	iterator_t *it = (iterator_t *)base;

	if (it->inode == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if (it->inode->base.type != SQFS_INODE_FILE &&
	    it->inode->base.type != SQFS_INODE_EXT_FILE) {
		return SQFS_ERROR_NOT_FILE;
	}

	if (it->data == NULL)
		return SQFS_ERROR_UNSUPPORTED;

	return sqfs_data_reader_create_stream(it->data, it->inode,
					      (const char *)it->dent->name,
					      out);
}

static int it_read_xattr(sqfs_dir_iterator_t *base, sqfs_xattr_t **out)
{
	iterator_t *it = (iterator_t *)base;

	if (it->inode == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if (it->xattr == NULL || it->xattr_idx == 0xFFFFFFFF)
		return 0;

	return sqfs_xattr_reader_read_all(it->xattr, it->xattr_idx, out);
}

static void it_destroy(sqfs_object_t *obj)
{
	iterator_t *it = (iterator_t *)obj;

	sqfs_free(it->inode);
	sqfs_free(it->dent);
	sqfs_drop(it->id);
	sqfs_drop(it->rd);
	sqfs_drop(it->data);
	sqfs_drop(it->xattr);
	sqfs_free(it);
}

int sqfs_dir_iterator_create(sqfs_dir_reader_t *rd,
			     sqfs_id_table_t *id,
			     sqfs_data_reader_t *data,
			     sqfs_xattr_reader_t *xattr,
			     const sqfs_inode_generic_t *inode,
			     sqfs_dir_iterator_t **out)
{
	sqfs_dir_iterator_t *base;
	iterator_t *it;
	int ret;

	it = calloc(1, sizeof(*it));
	base = (sqfs_dir_iterator_t *)it;

	if (it == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(it, it_destroy, NULL);

	ret = sqfs_dir_reader_open_dir(rd, inode, &it->state, 0);
	if (ret) {
		sqfs_free(it);
		return ret;
	}

	base->next = it_next;
	base->read_link = it_read_link;
	base->open_subdir = it_open_subdir;
	base->ignore_subdir = it_ignore_subdir;
	base->open_file_ro = it_open_file_ro;
	base->read_xattr = it_read_xattr;

	it->id = sqfs_grab(id);
	it->rd = sqfs_grab(rd);

	if (data != NULL)
		it->data = sqfs_grab(data);

	if (xattr != NULL)
		it->xattr = sqfs_grab(xattr);

	*out = base;
	return 0;
}
