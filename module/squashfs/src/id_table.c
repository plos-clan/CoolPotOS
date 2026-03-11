/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * id_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/id_table.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "compat.h"
#include "util/array.h"

#include "stdlib.h"
#include "string.h"

struct sqfs_id_table_t {
	sqfs_object_t base;

	array_t ids;
};

static void id_table_destroy(sqfs_object_t *obj)
{
	sqfs_id_table_t *tbl = (sqfs_id_table_t *)obj;

	array_cleanup(&tbl->ids);
	free(tbl);
}

static sqfs_object_t *id_table_copy(const sqfs_object_t *obj)
{
	const sqfs_id_table_t *tbl = (const sqfs_id_table_t *)obj;
	sqfs_id_table_t *copy = calloc(1, sizeof(*copy));

	if (copy == NULL)
		return NULL;

	if (array_init_copy(&copy->ids, &tbl->ids) != 0) {
		free(copy);
		return NULL;
	}

	return (sqfs_object_t *)copy;
}

sqfs_id_table_t *sqfs_id_table_create(sqfs_u32 flags)
{
	sqfs_id_table_t *tbl;

	if (flags != 0)
		return NULL;

	tbl = calloc(1, sizeof(sqfs_id_table_t));

	if (tbl != NULL) {
		array_init(&tbl->ids, sizeof(sqfs_u32), 0);
		sqfs_object_init(tbl, id_table_destroy, id_table_copy);
	}

	return tbl;
}

int sqfs_id_table_id_to_index(sqfs_id_table_t *tbl, sqfs_u32 id, sqfs_u16 *out)
{
	size_t i;

	for (i = 0; i < tbl->ids.used; ++i) {
		if (((sqfs_u32 *)tbl->ids.data)[i] == id) {
			*out = i;
			return 0;
		}
	}

	if (tbl->ids.used == 0x10000)
		return SQFS_ERROR_OVERFLOW;

	*out = tbl->ids.used;
	return array_append(&tbl->ids, &id);
}

int sqfs_id_table_index_to_id(const sqfs_id_table_t *tbl, sqfs_u16 index,
			      sqfs_u32 *out)
{
	if (index >= tbl->ids.used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	*out = ((sqfs_u32 *)tbl->ids.data)[index];
	return 0;
}

int sqfs_id_table_read(sqfs_id_table_t *tbl, sqfs_file_t *file,
		       const sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	sqfs_u64 upper_limit, lower_limit;
	void *raw_ids;
	size_t i;
	int ret;

	if (!super->id_count || super->id_table_start >= super->bytes_used)
		return SQFS_ERROR_CORRUPTED;

	upper_limit = super->id_table_start;
	lower_limit = super->directory_table_start;

	if (super->fragment_table_start > lower_limit &&
	    super->fragment_table_start < upper_limit) {
		lower_limit = super->fragment_table_start;
	}

	if (super->export_table_start > lower_limit &&
	    super->export_table_start < upper_limit) {
		lower_limit = super->export_table_start;
	}

	array_cleanup(&tbl->ids);
	tbl->ids.size = sizeof(sqfs_u32);

	ret = sqfs_read_table(file, cmp, super->id_count * sizeof(sqfs_u32),
			      super->id_table_start, lower_limit,
			      upper_limit, &raw_ids);
	if (ret)
		return ret;

	for (i = 0; i < super->id_count; ++i)
		((sqfs_u32 *)raw_ids)[i] = le32toh(((sqfs_u32 *)raw_ids)[i]);

	tbl->ids.data = raw_ids;
	tbl->ids.used = super->id_count;
	tbl->ids.count = super->id_count;
	return 0;
}

int sqfs_id_table_write(sqfs_id_table_t *tbl, sqfs_file_t *file,
			sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	sqfs_u64 start;
	size_t i;
	int ret;

	for (i = 0; i < tbl->ids.used; ++i) {
		((sqfs_u32 *)tbl->ids.data)[i] =
			htole32(((sqfs_u32 *)tbl->ids.data)[i]);
	}

	super->id_count = tbl->ids.used;

	ret = sqfs_write_table(file, cmp, tbl->ids.data,
			       sizeof(sqfs_u32) * tbl->ids.used, &start);

	super->id_table_start = start;

	for (i = 0; i < tbl->ids.used; ++i) {
		((sqfs_u32 *)tbl->ids.data)[i] =
			le32toh(((sqfs_u32 *)tbl->ids.data)[i]);
	}

	return ret;
}
