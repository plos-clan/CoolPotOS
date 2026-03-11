/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * backend.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int set_block_size(sqfs_inode_generic_t **inode,
			  sqfs_u32 index, sqfs_u32 size)
{
	size_t min_size = (index + 1) * sizeof(sqfs_u32);
	size_t avail = (*inode)->payload_bytes_available;
	sqfs_inode_generic_t *new;
	size_t newsz;

	if (avail < min_size) {
		newsz = avail ? avail : (sizeof(sqfs_u32) * 4);
		while (newsz < min_size)
			newsz *= 2;

		if (SZ_ADD_OV(newsz, sizeof(**inode), &newsz))
			return SQFS_ERROR_OVERFLOW;

		if (sizeof(size_t) > sizeof(sqfs_u32)) {
			if ((newsz - sizeof(**inode)) > 0x0FFFFFFFFUL)
				return SQFS_ERROR_OVERFLOW;
		}

		new = realloc((*inode), newsz);
		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		(*inode) = new;
		(*inode)->payload_bytes_available = newsz - sizeof(**inode);
	}

	(*inode)->extra[index] = size;

	if (min_size >= (*inode)->payload_bytes_used)
		(*inode)->payload_bytes_used = min_size;

	return 0;
}

static void release_old_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	blk->next = proc->free_list;
	proc->free_list = blk;

	proc->backlog -= 1;
}

static int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
		sqfs_block_t *it = proc->fblk_in_flight, *prev = NULL;

		while (it != NULL && it->index != blk->index) {
			prev = it;
			it = it->next;
		}

		if (it != NULL) {
			if (prev == NULL) {
				proc->fblk_in_flight = it->next;
			} else {
				prev->next = it->next;
			}
			free(it);
		}
	}

	err = proc->wr->write_data_block(proc->wr, blk->user, blk->size,
					 blk->checksum,
					 blk->flags & ~BLK_FLAG_INTERNAL,
					 blk->data, &location);
	if (err)
		goto out;

	proc->stats.output_bytes_generated += blk->size;

	if (blk->flags & SQFS_BLK_IS_SPARSE) {
		if (blk->inode != NULL) {
			sqfs_inode_make_extended(*(blk->inode));
			(*(blk->inode))->data.file_ext.sparse += blk->size;

			err = set_block_size(blk->inode, blk->index, 0);
			if (err)
				goto out;
		}
		proc->stats.sparse_block_count += 1;
	} else if (blk->size != 0) {
		size = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			size |= 1 << 24;

		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			if (proc->frag_tbl != NULL) {
				err = sqfs_frag_table_set(proc->frag_tbl,
							  blk->index, location,
							  size);
				if (err)
					goto out;
			}
			proc->stats.frag_block_count += 1;
		} else {
			if (blk->inode != NULL) {
				err = set_block_size(blk->inode, blk->index,
						     size);
				if (err)
					goto out;
			}
			proc->stats.data_block_count += 1;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK && blk->inode != NULL)
		sqfs_inode_set_file_block_start(*(blk->inode), location);
out:
	release_old_block(proc, blk);
	return err;
}

static int process_completed_fragment(sqfs_block_processor_t *proc,
				      sqfs_block_t *frag)
{
	chunk_info_t *chunk = NULL, search;
	struct hash_entry *entry;
	sqfs_u32 index, offset;
	int err;

	if (frag->flags & SQFS_BLK_IS_SPARSE) {
		if (frag->inode != NULL) {
			sqfs_inode_make_extended(*(frag->inode));
			set_block_size(frag->inode, frag->index, 0);
			(*(frag->inode))->data.file_ext.sparse += frag->size;
		}
		proc->stats.sparse_block_count += 1;
		release_old_block(proc, frag);
		return 0;
	}

	proc->stats.total_frag_count += 1;

	if (!(frag->flags & SQFS_BLK_DONT_DEDUPLICATE)) {
		search.hash = frag->checksum;
		search.size = frag->size;

		proc->current_frag = frag;
		proc->fblk_lookup_error = 0;
		entry = hash_table_search_pre_hashed(proc->frag_ht,
						     search.hash, &search);
		proc->current_frag = NULL;

		if (proc->fblk_lookup_error != 0) {
			err = proc->fblk_lookup_error;
			goto fail;
		}

		if (entry != NULL) {
			if (frag->inode != NULL) {
				chunk = entry->data;
				sqfs_inode_set_frag_location(*(frag->inode),
							     chunk->index,
							     chunk->offset);
			}
			release_old_block(proc, frag);
			return 0;
		}
	}

	if (proc->frag_block != NULL) {
		size_t size = proc->frag_block->size + frag->size;

		if (size > proc->max_block_size) {
			proc->frag_block->io_seq_num = proc->io_seq_num++;

			err = enqueue_block(proc, proc->frag_block);
			proc->frag_block = NULL;

			if (err)
				goto fail;
		}
	}

	if (proc->frag_block == NULL) {
		if (proc->frag_tbl == NULL) {
			index = 0;
		} else {
			err = sqfs_frag_table_append(proc->frag_tbl,
						     0, 0, &index);
			if (err)
				goto fail;
		}

		offset = 0;
		proc->frag_block = frag;
		proc->frag_block->index = index;
		proc->frag_block->flags &= SQFS_BLK_DONT_COMPRESS;
		proc->frag_block->flags |= SQFS_BLK_FRAGMENT_BLOCK;
	} else {
		index = proc->frag_block->index;
		offset = proc->frag_block->size;

		memcpy(proc->frag_block->data + proc->frag_block->size,
		       frag->data, frag->size);

		proc->frag_block->size += frag->size;
		proc->frag_block->flags |=
			(frag->flags & SQFS_BLK_DONT_COMPRESS);
	}

	if (proc->frag_tbl != NULL) {
		err = SQFS_ERROR_ALLOC;
		chunk = calloc(1, sizeof(*chunk));
		if (chunk == NULL)
			goto fail;

		chunk->index = index;
		chunk->offset = offset;
		chunk->size = frag->size;
		chunk->hash = frag->checksum;

		proc->current_frag = frag;
		proc->fblk_lookup_error = 0;
		entry = hash_table_insert_pre_hashed(proc->frag_ht, chunk->hash,
						     chunk, chunk);
		proc->current_frag = NULL;

		if (proc->fblk_lookup_error != 0) {
			err = proc->fblk_lookup_error;
			goto fail;
		}

		if (entry == NULL)
			goto fail;
	}

	if (frag->inode != NULL)
		sqfs_inode_set_frag_location(*(frag->inode), index, offset);

	if (frag != proc->frag_block)
		release_old_block(proc, frag);

	proc->stats.actual_frag_count += 1;
	return 0;
fail:
	free(chunk);
	if (frag != proc->frag_block)
		release_old_block(proc, frag);
	return err;
}

static void store_io_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_block_t *prev = NULL, *it = proc->io_queue;

	while (it != NULL && (it->io_seq_num < blk->io_seq_num)) {
		prev = it;
		it = it->next;
	}

	if (prev == NULL) {
		proc->io_queue = blk;
	} else {
		prev->next = blk;
	}

	blk->next = it;
}

int dequeue_block(sqfs_block_processor_t *proc)
{
	size_t backlog_old = proc->backlog;
	sqfs_block_t *blk;
	int status;

	do {
		while (proc->io_queue != NULL) {
			if (proc->io_queue->io_seq_num != proc->io_deq_seq_num)
				break;

			blk = proc->io_queue;
			proc->io_queue = blk->next;
			proc->io_deq_seq_num += 1;

			status = process_completed_block(proc, blk);
			if (status != 0)
				return status;
		}

		if (proc->backlog < backlog_old)
			break;

		if ((proc->backlog == 1) &&
		    (proc->frag_block != NULL || proc->blk_current != NULL)) {
			break;
		}

		if ((proc->backlog == 2) &&
		    proc->frag_block != NULL && proc->blk_current != NULL) {
			break;
		}

		blk = proc->pool->dequeue(proc->pool);

		if (blk == NULL) {
			status = proc->pool->get_status(proc->pool);
			return status ? status : SQFS_ERROR_INTERNAL;
		}

		if (blk->flags & SQFS_BLK_IS_FRAGMENT) {
			status = process_completed_fragment(proc, blk);
			if (status != 0)
				return status;
		} else {
			if (!(blk->flags & SQFS_BLK_FRAGMENT_BLOCK) ||
			    (blk->flags & BLK_FLAG_MANUAL_SUBMISSION)) {
				blk->io_seq_num = proc->io_seq_num++;
			}

			store_io_block(proc, blk);
		}
	} while (proc->backlog >= backlog_old);

	return 0;
}
