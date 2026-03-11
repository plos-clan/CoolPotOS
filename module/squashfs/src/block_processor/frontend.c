/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * frontend.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int get_new_block(sqfs_block_processor_t *proc, sqfs_block_t **out)
{
	sqfs_block_t *blk;

	while (proc->backlog >= proc->max_backlog) {
		int ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	if (proc->free_list != NULL) {
		blk = proc->free_list;
		proc->free_list = blk->next;
	} else {
		blk = malloc(sizeof(*blk) + proc->max_block_size);
		if (blk == NULL)
			return SQFS_ERROR_ALLOC;
	}

	memset(blk, 0, sizeof(*blk));
	*out = blk;

	proc->backlog += 1;
	return 0;
}

static int add_sentinel_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;
	int ret;

	ret = get_new_block(proc, &blk);
	if (ret != 0)
		return ret;

	blk->inode = proc->inode;
	blk->flags = proc->blk_flags | SQFS_BLK_LAST_BLOCK;

	return enqueue_block(proc, blk);
}

int enqueue_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	int status;

	if ((blk->flags & SQFS_BLK_FRAGMENT_BLOCK) &&
	    proc->file != NULL && proc->uncmp != NULL) {
		sqfs_block_t *copy = alloc_flex(sizeof(*copy), 1, blk->size);

		if (copy == NULL)
			return SQFS_ERROR_ALLOC;

		copy->size = blk->size;
		copy->index = blk->index;
		memcpy(copy->data, blk->data, blk->size);

		copy->next = proc->fblk_in_flight;
		proc->fblk_in_flight = copy;
	}

	if (proc->pool->submit(proc->pool, blk) != 0) {
		status = proc->pool->get_status(proc->pool);

		if (status == 0)
			status = SQFS_ERROR_ALLOC;

		blk->next = proc->free_list;
		proc->free_list = blk;
		return status;
	}

	return 0;
}

int sqfs_block_processor_begin_file(sqfs_block_processor_t *proc,
				    sqfs_inode_generic_t **inode,
				    void *user, sqfs_u32 flags)
{
	if (proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (flags & ~SQFS_BLK_USER_SETTABLE_FLAGS)
		return SQFS_ERROR_UNSUPPORTED;

	if (inode != NULL) {
		(*inode) = calloc(1, sizeof(sqfs_inode_generic_t));
		if ((*inode) == NULL)
			return SQFS_ERROR_ALLOC;

		(*inode)->base.type = SQFS_INODE_FILE;
		sqfs_inode_set_frag_location(*inode, 0xFFFFFFFF, 0xFFFFFFFF);
	}

	proc->begin_called = true;
	proc->inode = inode;
	proc->blk_flags = flags | SQFS_BLK_FIRST_BLOCK;
	proc->blk_index = 0;
	proc->user = user;
	return 0;
}

int sqfs_block_processor_append(sqfs_block_processor_t *proc, const void *data,
				size_t size)
{
	sqfs_block_t *new;
	sqfs_u64 filesize;
	size_t diff;
	void *dst;
	int err;

	if (!proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (proc->inode != NULL) {
		sqfs_inode_get_file_size(*(proc->inode), &filesize);
		sqfs_inode_set_file_size(*(proc->inode), filesize + size);
	}

	while (size > 0) {
		if (proc->blk_current == NULL) {
			err = get_new_block(proc, &new);
			if (err != 0)
				return err;

			proc->blk_current = new;
			proc->blk_current->flags = proc->blk_flags;
			proc->blk_current->inode = proc->inode;
			proc->blk_current->user = proc->user;
			proc->blk_current->index = proc->blk_index++;
			proc->blk_flags &= ~SQFS_BLK_FIRST_BLOCK;
		}

		diff = proc->max_block_size - proc->blk_current->size;

		if (diff == 0) {
			err = enqueue_block(proc, proc->blk_current);
			proc->blk_current = NULL;

			if (err)
				return err;
			continue;
		}

		if (diff > size)
			diff = size;

		dst = proc->blk_current->data + proc->blk_current->size;

		if (data == NULL) {
			memset(dst, 0, diff);
		} else {
			memcpy(dst, data, diff);
			data = (const char *)data + diff;
		}

		size -= diff;
		proc->blk_current->size += diff;

		proc->stats.input_bytes_read += diff;
	}

	if (proc->blk_current->size == proc->max_block_size) {
		err = enqueue_block(proc, proc->blk_current);
		proc->blk_current = NULL;

		if (err)
			return err;
	}

	return 0;
}

int sqfs_block_processor_end_file(sqfs_block_processor_t *proc)
{
	int err;

	if (!proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (proc->blk_current == NULL) {
		if (!(proc->blk_flags & SQFS_BLK_FIRST_BLOCK)) {
			err = add_sentinel_block(proc);
			if (err)
				return err;
		}
	} else {
		if (proc->blk_flags & SQFS_BLK_DONT_FRAGMENT) {
			proc->blk_current->flags |= SQFS_BLK_LAST_BLOCK;
		} else {
			if (!(proc->blk_current->flags &
			      SQFS_BLK_FIRST_BLOCK)) {
				err = add_sentinel_block(proc);
				if (err)
					return err;
			}

			proc->blk_current->flags |= SQFS_BLK_IS_FRAGMENT;
		}

		err = enqueue_block(proc, proc->blk_current);
		proc->blk_current = NULL;

		if (err)
			return err;
	}

	proc->begin_called = false;
	proc->inode = NULL;
	proc->user = NULL;
	proc->blk_flags = 0;
	return 0;
}

int sqfs_block_processor_submit_block(sqfs_block_processor_t *proc, void *user,
				      sqfs_u32 flags, const void *data,
				      size_t size)
{
	sqfs_block_t *blk;
	int ret;

	if (proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (size > proc->max_block_size)
		return SQFS_ERROR_OVERFLOW;

	if (flags & ~SQFS_BLK_FLAGS_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	ret = get_new_block(proc, &blk);
	if (ret != 0)
		return ret;

	blk->flags = flags | BLK_FLAG_MANUAL_SUBMISSION;
	blk->user = user;
	blk->size = size;
	memcpy(blk->data, data, size);

	return enqueue_block(proc, blk);
}
