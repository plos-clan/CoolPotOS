/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * frag_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/frag_table.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "compat.h"
#include "util/array.h"

#include <stdlib.h>
#include <string.h>

struct sqfs_frag_table_t {
	sqfs_object_t base;

	array_t table;
};

static void frag_table_destroy(sqfs_object_t *obj)
{
	sqfs_frag_table_t *tbl = (sqfs_frag_table_t *)obj;

	array_cleanup(&tbl->table);
	free(tbl);
}

static sqfs_object_t *frag_table_copy(const sqfs_object_t *obj)
{
	const sqfs_frag_table_t *tbl = (const sqfs_frag_table_t *)obj;
	sqfs_frag_table_t *copy = calloc(1, sizeof(*copy));

	if (copy == NULL)
		return NULL;

	if (array_init_copy(&copy->table, &tbl->table)) {
		free(copy);
		return NULL;
	}

	return (sqfs_object_t *)copy;
}

sqfs_frag_table_t *sqfs_frag_table_create(sqfs_u32 flags)
{
	sqfs_frag_table_t *tbl;

	if (flags != 0)
		return NULL;

	tbl = calloc(1, sizeof(*tbl));
	if (tbl == NULL)
		return NULL;

	sqfs_object_init(tbl, frag_table_destroy, frag_table_copy);

	array_init(&tbl->table, sizeof(sqfs_fragment_t), 0);
	return tbl;
}

int sqfs_frag_table_read(sqfs_frag_table_t *tbl, sqfs_file_t *file,
			 const sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	sqfs_u64 location, lower, upper;
	void *raw = NULL;
	size_t size;
	int err;

	array_cleanup(&tbl->table);
	tbl->table.size = sizeof(sqfs_fragment_t);

	if (super->flags & SQFS_FLAG_NO_FRAGMENTS)
		return 0;

	if (super->fragment_table_start == 0xFFFFFFFFFFFFFFFFUL)
		return 0;

	if (super->fragment_entry_count == 0)
		return 0;

	if (super->fragment_table_start >= super->bytes_used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	/* location must be after inode & directory table,
	   but before the ID table */
	if (super->fragment_table_start < super->directory_table_start)
		return SQFS_ERROR_CORRUPTED;

	if (super->fragment_table_start >= super->id_table_start)
		return SQFS_ERROR_CORRUPTED;

	location = super->fragment_table_start;
	lower = super->directory_table_start;
	upper = super->id_table_start;

	if (super->export_table_start < super->id_table_start)
		upper = super->export_table_start;

	if (SZ_MUL_OV(super->fragment_entry_count, sizeof(sqfs_fragment_t),
		      &size)) {
		return SQFS_ERROR_OVERFLOW;
	}

	err = sqfs_read_table(file, cmp, size, location, lower, upper, &raw);
	if (err) {
		free(raw);
		return err;
	}

	tbl->table.data = raw;
	tbl->table.count = super->fragment_entry_count;
	tbl->table.used = super->fragment_entry_count;
	return 0;
}

int sqfs_frag_table_write(sqfs_frag_table_t *tbl, sqfs_file_t *file,
			  sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	size_t i;
	int err;

	if (tbl->table.used == 0) {
		super->fragment_table_start = 0xFFFFFFFFFFFFFFFF;
		super->flags |= SQFS_FLAG_NO_FRAGMENTS;
		super->flags &= ~SQFS_FLAG_ALWAYS_FRAGMENTS;
		super->flags &= ~SQFS_FLAG_UNCOMPRESSED_FRAGMENTS;
		return 0;
	}

	err = sqfs_write_table(file, cmp, tbl->table.data,
			       tbl->table.size * tbl->table.used,
			       &super->fragment_table_start);
	if (err)
		return err;

	super->fragment_entry_count = tbl->table.used;
	super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	super->flags |= SQFS_FLAG_UNCOMPRESSED_FRAGMENTS;

	for (i = 0; i < tbl->table.used; ++i) {
		sqfs_u32 sz = ((sqfs_fragment_t *)tbl->table.data)[i].size;

		if (SQFS_IS_BLOCK_COMPRESSED(le32toh(sz))) {
			super->flags &= ~SQFS_FLAG_UNCOMPRESSED_FRAGMENTS;
			break;
		}
	}

	return 0;
}

int sqfs_frag_table_lookup(sqfs_frag_table_t *tbl, sqfs_u32 index,
			   sqfs_fragment_t *out)
{
	sqfs_fragment_t *frag = array_get(&tbl->table, index);

	if (frag == NULL)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	out->start_offset = le64toh(frag->start_offset);
	out->size = le32toh(frag->size);
	out->pad0 = le32toh(frag->pad0);
	return 0;
}

int sqfs_frag_table_append(sqfs_frag_table_t *tbl, sqfs_u64 location,
			   sqfs_u32 size, sqfs_u32 *index)
{
	sqfs_fragment_t frag;

	if (index != NULL)
		*index = tbl->table.used;

	memset(&frag, 0, sizeof(frag));
	frag.start_offset = htole64(location);
	frag.size = htole32(size);

	return array_append(&tbl->table, &frag);
}

int sqfs_frag_table_set(sqfs_frag_table_t *tbl, sqfs_u32 index,
			sqfs_u64 location, sqfs_u32 size)
{
	sqfs_fragment_t frag;

	memset(&frag, 0, sizeof(frag));
	frag.start_offset = htole64(location);
	frag.size = htole32(size);

	return array_set(&tbl->table, index, &frag);
}

size_t sqfs_frag_table_get_size(sqfs_frag_table_t *tbl)
{
	return tbl->table.used;
}
