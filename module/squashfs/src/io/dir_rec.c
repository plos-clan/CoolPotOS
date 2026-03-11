/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_rec.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "util/util.h"
#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#include <stdlib.h>
#include <string.h>

typedef struct dir_stack_t {
	struct dir_stack_t *next;
	sqfs_dir_iterator_t *dir;
	char name[];
} dir_stack_t;

typedef struct {
	sqfs_dir_iterator_t base;

	int state;
	dir_stack_t *top;

	dir_stack_t *next_top;
} dir_tree_iterator_t;

static void pop(dir_tree_iterator_t *it)
{
	if (it->top != NULL) {
		dir_stack_t *ent = it->top;
		it->top = it->top->next;

		sqfs_drop(ent->dir);
		free(ent);
	}
}

static sqfs_dir_entry_t *expand_path(const dir_tree_iterator_t *it,
				     sqfs_dir_entry_t *ent)
{
	size_t slen = strlen(ent->name) + 1, plen = 0;
	char *dst;
	void *new;

	for (dir_stack_t *sit = it->top; sit != NULL; sit = sit->next) {
		if (sit->name[0] != '\0')
			plen += strlen(sit->name) + 1;
	}

	if (plen == 0)
		return ent;

	new = realloc(ent, sizeof(*ent) + plen + slen);
	if (new == NULL) {
		free(ent);
		return NULL;
	}

	ent = new;
	memmove(ent->name + plen, ent->name, slen);
	dst = ent->name + plen;

	for (dir_stack_t *sit = it->top; sit != NULL; sit = sit->next) {
		size_t len = strlen(sit->name);
		if (len > 0) {
			*(--dst) = '/';
			dst -= len;
			memcpy(dst, sit->name, len);
		}
	}

	return ent;
}

/*****************************************************************************/

static void destroy(sqfs_object_t *obj)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)obj;

	while (it->top != NULL)
		pop(it);

	if (it->next_top != NULL) {
		sqfs_drop(it->next_top->dir);
		free(it->next_top);
	}

	free(it);
}

static int next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;
	sqfs_dir_entry_t *ent = NULL;
	int ret;

	*out = NULL;
	if (it->state != 0)
		return it->state;

	if (it->next_top != NULL) {
		it->next_top->next = it->top;
		it->top = it->next_top;
		it->next_top = NULL;
	}

	for (;;) {
		if (it->top == NULL) {
			ret = 1;
			goto fail;
		}

		ret = it->top->dir->next(it->top->dir, &ent);
		if (ret < 0)
			goto fail;

		if (ret > 0) {
			pop(it);
			continue;
		}

		if (!strcmp(ent->name, ".") || !strcmp(ent->name, "..")) {
			free(ent);
			ent = NULL;
			continue;
		}

		break;
	}

	ent = expand_path(it, ent);
	if (ent == NULL) {
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	if (S_ISDIR(ent->mode)) {
		sqfs_dir_iterator_t *sub = NULL;
		const char *name = strrchr(ent->name, '/');
		name = (name == NULL) ? ent->name : (name + 1);

		ret = it->top->dir->open_subdir(it->top->dir, &sub);
		if (ret != 0)
			goto fail;

		it->next_top = alloc_flex(sizeof(*(it->next_top)), 1,
					  strlen(name) + 1);
		if (it->next_top == NULL) {
			sqfs_drop(sub);
			goto fail;
		}

		strcpy(it->next_top->name, name);
		it->next_top->dir = sub;
	}

	*out = ent;
	return it->state;
fail:
	free(ent);
	it->state = ret;
	return it->state;
}

static int read_link(sqfs_dir_iterator_t *base, char **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	*out = NULL;
	if (it->top == NULL)
		return SQFS_ERROR_NO_ENTRY;

	return it->top->dir->read_link(it->top->dir, out);
}

static int open_subdir(sqfs_dir_iterator_t *base, sqfs_dir_iterator_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	*out = NULL;
	if (it->top == NULL)
		return SQFS_ERROR_NO_ENTRY;

	return it->top->dir->open_subdir(it->top->dir, out);
}

static void ignore_subdir(sqfs_dir_iterator_t *base)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->next_top != NULL) {
		sqfs_drop(it->next_top->dir);
		free(it->next_top);
		it->next_top = NULL;
	}
}

static int open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	*out = NULL;
	if (it->top == NULL)
		return SQFS_ERROR_NO_ENTRY;

	return it->top->dir->open_file_ro(it->top->dir, out);
}

static int read_xattr(sqfs_dir_iterator_t *base, sqfs_xattr_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	*out = NULL;
	if (it->top == NULL)
		return SQFS_ERROR_NO_ENTRY;

	return it->top->dir->read_xattr(it->top->dir, out);
}

int sqfs_dir_iterator_create_recursive(sqfs_dir_iterator_t **out,
				       sqfs_dir_iterator_t *base)
{
	dir_tree_iterator_t *it = calloc(1, sizeof(*it));

	*out = NULL;
	if (it == NULL)
		return SQFS_ERROR_ALLOC;

	it->next_top = calloc(1, sizeof(*(it->next_top)) + 1);
	if (it->next_top == NULL) {
		free(it);
		return SQFS_ERROR_ALLOC;
	}
	it->next_top->dir = sqfs_grab(base);

	sqfs_object_init(it, destroy, NULL);
	((sqfs_dir_iterator_t *)it)->next = next;
	((sqfs_dir_iterator_t *)it)->read_link = read_link;
	((sqfs_dir_iterator_t *)it)->open_subdir = open_subdir;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = read_xattr;

	*out = (sqfs_dir_iterator_t *)it;
	return 0;
}
