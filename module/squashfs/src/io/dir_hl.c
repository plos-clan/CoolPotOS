/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_hl.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "util/util.h"
#include "util/rbtree.h"
#include "compat.h"

#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	sqfs_u64 dev;
	sqfs_u64 inum;
} inumtree_key_t;

typedef struct {
	sqfs_dir_iterator_t base;

	int state;
	const char *link_target;
	sqfs_dir_iterator_t *src;
	rbtree_t inumtree;
} hl_iterator_t;

static int compare_inum(const void *ctx, const void *lhs, const void *rhs)
{
	const inumtree_key_t *l = (const inumtree_key_t *)lhs;
	const inumtree_key_t *r = (const inumtree_key_t *)rhs;
	(void)ctx;

	if (l->dev != r->dev)
		return l->dev < r->dev ? -1 : 1;

	return l->inum < r->inum ? -1 : (l->inum > r->inum ? 1 : 0);
}

static const char *detect_hard_link(const hl_iterator_t *it,
				    const sqfs_dir_entry_t *ent)
{
	inumtree_key_t key = { ent->dev, ent->inode };
	rbtree_node_t *tn;

	if (S_ISDIR(ent->mode))
		return NULL;

	tn = rbtree_lookup(&(it->inumtree), &key);
	if (tn == NULL)
		return NULL;

	return *((const char **)rbtree_node_value(tn));
}

static int store_hard_link(hl_iterator_t *it,
			   const sqfs_dir_entry_t *ent)
{
	inumtree_key_t key = { ent->dev, ent->inode };
	char *target;
	int ret;

	if (S_ISDIR(ent->mode) || (ent->flags & SQFS_DIR_ENTRY_FLAG_HARD_LINK))
		return 0;

	target = strdup(ent->name);
	if (target == NULL)
		return SQFS_ERROR_ALLOC;

	ret = rbtree_insert(&(it->inumtree), &key, &target);
	if (ret != 0)
		free(target);

	return ret;
}

static void remove_links(rbtree_node_t *n)
{
	if (n != NULL) {
		char **lnk = rbtree_node_value(n);
		free(*lnk);
		*lnk = NULL;

		remove_links(n->left);
		remove_links(n->right);
	}
}

/*****************************************************************************/

static void destroy(sqfs_object_t *obj)
{
	hl_iterator_t *it = (hl_iterator_t *)obj;

	remove_links(it->inumtree.root);
	rbtree_cleanup(&it->inumtree);
	sqfs_drop(it->src);
	free(it);
}

static int next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	hl_iterator_t *it = (hl_iterator_t *)base;
	int ret;

	if (it->state != 0) {
		*out = NULL;
		return it->state;
	}

	ret = it->src->next(it->src, out);
	if (ret != 0) {
		*out = NULL;
		it->link_target = NULL;
		it->state = ret;
		return ret;
	}

	it->link_target = detect_hard_link(it, *out);

	if (it->link_target == NULL) {
		ret = store_hard_link(it, *out);
		if (ret != 0) {
			it->state = ret;
			sqfs_free(*out);
			*out = NULL;
		}
	} else {
		(*out)->size = strlen(it->link_target);
		(*out)->mode = SQFS_INODE_MODE_LNK | 0777;
		(*out)->flags |= SQFS_DIR_ENTRY_FLAG_HARD_LINK;
	}

	return ret;
}

static int read_link(sqfs_dir_iterator_t *base, char **out)
{
	hl_iterator_t *it = (hl_iterator_t *)base;

	if (it->link_target != NULL) {
		*out = strdup(it->link_target);
		if (*out == NULL)
			return SQFS_ERROR_ALLOC;
		return 0;
	}

	if (it->state != 0)
		return SQFS_ERROR_NO_ENTRY;

	return it->src->read_link(it->src, out);
}

static int open_subdir(sqfs_dir_iterator_t *base, sqfs_dir_iterator_t **out)
{
	hl_iterator_t *it = (hl_iterator_t *)base;

	if (it->link_target != NULL) {
		*out = NULL;
		return SQFS_ERROR_NOT_DIR;
	}

	if (it->state != 0)
		return SQFS_ERROR_NO_ENTRY;

	return it->src->open_subdir(it->src, out);
}

static void ignore_subdir(sqfs_dir_iterator_t *base)
{
	hl_iterator_t *it = (hl_iterator_t *)base;

	if (it->link_target == NULL && it->state == 0)
		it->src->ignore_subdir(it->src);
}

static int open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	hl_iterator_t *it = (hl_iterator_t *)base;

	if (it->link_target != NULL) {
		*out = NULL;
		return SQFS_ERROR_NOT_FILE;
	}

	if (it->state != 0)
		return SQFS_ERROR_NO_ENTRY;

	return it->src->open_file_ro(it->src, out);
}

static int read_xattr(sqfs_dir_iterator_t *base, sqfs_xattr_t **out)
{
	hl_iterator_t *it = (hl_iterator_t *)base;

	if (it->link_target != NULL) {
		*out = NULL;
		return 0;
	}

	if (it->state != 0)
		return SQFS_ERROR_NO_ENTRY;

	return it->src->read_xattr(it->src, out);
}

int sqfs_hard_link_filter_create(sqfs_dir_iterator_t **out,
				 sqfs_dir_iterator_t *base)
{
	hl_iterator_t *it;
	int ret;

	*out = NULL;

	it = calloc(1, sizeof(*it));
	if (it == NULL)
		return SQFS_ERROR_ALLOC;

	ret = rbtree_init(&it->inumtree, sizeof(inumtree_key_t),
			  sizeof(char *), compare_inum);
	if (ret != 0) {
		free(it);
		return ret;
	}

	sqfs_object_init(it, destroy, NULL);
	((sqfs_dir_iterator_t *)it)->next = next;
	((sqfs_dir_iterator_t *)it)->read_link = read_link;
	((sqfs_dir_iterator_t *)it)->open_subdir = open_subdir;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = read_xattr;

	it->src = sqfs_grab(base);

	*out = (sqfs_dir_iterator_t *)it;
	return 0;
}
