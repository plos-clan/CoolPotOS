/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "sqfs/meta_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/super.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"
#include "util/rbtree.h"
#include "util/util.h"

#include <string.h>
#include <stdlib.h>

enum {
	DIR_STATE_NONE = 0,
	DIR_STATE_OPENED = 1,
	DIR_STATE_DOT = 2,
	DIR_STATE_ENTRIES = 3,
};

struct sqfs_dir_reader_t {
	sqfs_object_t base;

	sqfs_meta_reader_t *meta_dir;
	sqfs_meta_reader_t *meta_inode;
	sqfs_super_t super;

	sqfs_u32 flags;

	rbtree_t dcache;
};

static int dcache_key_compare(const void *ctx, const void *l, const void *r)
{
	sqfs_u32 lhs = *((const sqfs_u32 *)l), rhs = *((const sqfs_u32 *)r);
	(void)ctx;

	return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static int dcache_add(sqfs_dir_reader_t *rd,
		      const sqfs_inode_generic_t *inode, sqfs_u64 ref)
{
	sqfs_u32 inum = inode->base.inode_number;

	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return 0;

	if (inode->base.type != SQFS_INODE_DIR &&
	    inode->base.type != SQFS_INODE_EXT_DIR) {
		return 0;
	}

	if (rbtree_lookup(&rd->dcache, &inum) != NULL)
		return 0;

	return rbtree_insert(&rd->dcache, &inum, &ref);
}

static void dir_reader_destroy(sqfs_object_t *obj)
{
	sqfs_dir_reader_t *rd = (sqfs_dir_reader_t *)obj;

	if (rd->flags & SQFS_DIR_READER_DOT_ENTRIES)
		rbtree_cleanup(&rd->dcache);

	sqfs_drop(rd->meta_inode);
	sqfs_drop(rd->meta_dir);
	free(rd);
}

static sqfs_object_t *dir_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_dir_reader_t *rd = (const sqfs_dir_reader_t *)obj;
	sqfs_dir_reader_t *copy  = malloc(sizeof(*copy));

	if (copy == NULL)
		return NULL;

	memcpy(copy, rd, sizeof(*copy));

	if (rd->flags & SQFS_DIR_READER_DOT_ENTRIES) {
		if (rbtree_copy(&rd->dcache, &copy->dcache))
			goto fail_cache;
	}

	copy->meta_inode = sqfs_copy(rd->meta_inode);
	if (copy->meta_inode == NULL)
		goto fail_mino;

	copy->meta_dir = sqfs_copy(rd->meta_dir);
	if (copy->meta_dir == NULL)
		goto fail_mdir;

	return (sqfs_object_t *)copy;
fail_mdir:
	sqfs_drop(copy->meta_inode);
fail_mino:
	if (copy->flags & SQFS_DIR_READER_DOT_ENTRIES)
		rbtree_cleanup(&copy->dcache);
fail_cache:
	free(copy);
	return NULL;
}

sqfs_dir_reader_t *sqfs_dir_reader_create(const sqfs_super_t *super,
					  sqfs_compressor_t *cmp,
					  sqfs_file_t *file,
					  sqfs_u32 flags)
{
	sqfs_dir_reader_t *rd;
	sqfs_u64 start, limit;
	int ret;

	if (flags & ~SQFS_DIR_READER_ALL_FLAGS)
		return NULL;

	rd = calloc(1, sizeof(*rd));
	if (rd == NULL)
		return NULL;

	sqfs_object_init(rd, dir_reader_destroy, dir_reader_copy);

	if (flags & SQFS_DIR_READER_DOT_ENTRIES) {
		ret = rbtree_init(&rd->dcache, sizeof(sqfs_u32),
				  sizeof(sqfs_u64), dcache_key_compare);

		if (ret != 0)
			goto fail_dcache;
	}

	start = super->inode_table_start;
	limit = super->directory_table_start;

	rd->meta_inode = sqfs_meta_reader_create(file, cmp, start, limit);
	if (rd->meta_inode == NULL)
		goto fail_mino;

	start = super->directory_table_start;
	limit = super->id_table_start;

	if (super->fragment_table_start < limit)
		limit = super->fragment_table_start;

	if (super->export_table_start < limit)
		limit = super->export_table_start;

	rd->meta_dir = sqfs_meta_reader_create(file, cmp, start, limit);
	if (rd->meta_dir == NULL)
		goto fail_mdir;

	rd->super = *super;
	rd->flags = flags;
	return rd;
fail_mdir:
	sqfs_drop(rd->meta_inode);
fail_mino:
	if (flags & SQFS_DIR_READER_DOT_ENTRIES)
		rbtree_cleanup(&rd->dcache);
fail_dcache:
	free(rd);
	return NULL;
}

int sqfs_dir_reader_open_dir(sqfs_dir_reader_t *rd,
			     const sqfs_inode_generic_t *inode,
			     sqfs_dir_reader_state_t *state,
			     sqfs_u32 flags)
{
	sqfs_u32 parent;
	int ret;

	if (flags & (~SQFS_DIR_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	memset(state, 0, sizeof(*state));

	ret = sqfs_readdir_state_init(&state->cursor, &rd->super, inode);
	if (ret)
		return ret;

	if ((rd->flags & SQFS_DIR_READER_DOT_ENTRIES) &&
	    !(flags & SQFS_DIR_OPEN_NO_DOT_ENTRIES)) {
		if (inode->base.type == SQFS_INODE_EXT_DIR) {
			parent = inode->data.dir_ext.parent_inode;
		} else {
			parent = inode->data.dir.parent_inode;
		}

		ret = sqfs_dir_reader_resolve_inum(rd, inode->base.inode_number,
						   &state->dir_ref);
		if (ret)
			return ret;

		if (state->dir_ref == rd->super.root_inode_ref) {
			state->parent_ref = state->dir_ref;
		} else if (sqfs_dir_reader_resolve_inum(rd, parent,
							&state->parent_ref)) {
			return SQFS_ERROR_NO_ENTRY;
		}

		state->state = DIR_STATE_OPENED;
	} else {
		state->state = DIR_STATE_ENTRIES;
	}

	return 0;
}

static int mk_dummy_entry(const char *str, sqfs_dir_node_t **out)
{
	size_t len = strlen(str);
	sqfs_dir_node_t *ent;

	ent = calloc(1, sizeof(sqfs_dir_node_t) + len + 1);
	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	ent->type = SQFS_INODE_DIR;
	ent->size = len - 1;

	strcpy((char *)ent->name, str);

	*out = ent;
	return 0;
}

int sqfs_dir_reader_read(sqfs_dir_reader_t *rd, sqfs_dir_reader_state_t *state,
			 sqfs_dir_node_t **out)
{
	int err;

	switch (state->state) {
	case DIR_STATE_OPENED:
		err = mk_dummy_entry(".", out);
		if (err == 0) {
			state->state = DIR_STATE_DOT;
			state->ent_ref = state->dir_ref;
		}
		return err;
	case DIR_STATE_DOT:
		err = mk_dummy_entry("..", out);
		if (err == 0) {
			state->state = DIR_STATE_ENTRIES;
			state->ent_ref = state->parent_ref;
		}
		return err;
	case DIR_STATE_ENTRIES:
		break;
	default:
		return SQFS_ERROR_SEQUENCE;
	}

	return sqfs_meta_reader_readdir(rd->meta_dir, &state->cursor,
					out, NULL, &state->ent_ref);
}

int sqfs_dir_reader_get_inode(sqfs_dir_reader_t *rd, sqfs_u64 ref,
			      sqfs_inode_generic_t **inode)
{
	int ret;

	ret = sqfs_meta_reader_read_inode(rd->meta_inode, &rd->super,
					  ref >> 16, ref & 0x0FFFF, inode);
	if (ret != 0)
		return ret;

	return dcache_add(rd, *inode, ref);
}

int sqfs_dir_reader_get_root_inode(sqfs_dir_reader_t *rd,
				   sqfs_inode_generic_t **inode)
{
	return sqfs_dir_reader_get_inode(rd, rd->super.root_inode_ref, inode);
}

int sqfs_dir_reader_resolve_inum(sqfs_dir_reader_t *rd,
				 sqfs_u32 inode, sqfs_u64 *ref)
{
	rbtree_node_t *node;

	*ref = 0;
	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return SQFS_ERROR_NO_ENTRY;

	node = rbtree_lookup(&rd->dcache, &inode);
	if (node == NULL)
		return SQFS_ERROR_NO_ENTRY;

	*ref = *((sqfs_u64 *)rbtree_node_value(node));
	return 0;
}

int sqfs_dir_reader_resolve_path(sqfs_dir_reader_t *rd, const char *path,
				 const sqfs_inode_generic_t *root,
				 sqfs_u64 *out)
{
	bool is_first = true;

	while (path != NULL && *path != '\0') {
		sqfs_dir_reader_state_t state;
		int ret;

		if (*path == '/') {
			while (*path == '/')
				++path;
			continue;
		}

		if (is_first && root != NULL) {
			ret = sqfs_dir_reader_open_dir(rd, root, &state, 0);
		} else {
			sqfs_inode_generic_t *inode = NULL;

			if (is_first)
				*out = rd->super.root_inode_ref;

			ret = sqfs_dir_reader_get_inode(rd, *out, &inode);
			if (ret == 0) {
				ret = sqfs_dir_reader_open_dir(rd, inode,
							       &state, 0);
			}

			sqfs_free(inode);
		}

		is_first = false;
		if (ret != 0)
			return ret;

		for (;;) {
			sqfs_dir_node_t *ent;
			size_t len;

			ret = sqfs_dir_reader_read(rd, &state, &ent);
			if (ret < 0)
				return ret;
			if (ret > 0)
				return SQFS_ERROR_NO_ENTRY;

			len = ent->size + 1;
			ret = strncmp((const char *)ent->name, path, len);
			sqfs_free(ent);

			if (ret == 0 &&
			    (path[len] == '/' || path[len] == '\0')) {
				path += len;
				break;
			}
		}

		*out = state.ent_ref;
	}

	if (is_first) {
		if (root == NULL) {
			*out = rd->super.root_inode_ref;
			return 0;
		}

		return sqfs_dir_reader_resolve_inum(rd,
						    root->base.inode_number,
						    out);
	}

	return 0;
}
