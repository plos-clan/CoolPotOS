/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * write_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/error.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <stdlib.h>

int sqfs_write_table(sqfs_file_t *file, sqfs_compressor_t *cmp,
		     const void *data, size_t table_size, sqfs_u64 *start)
{
	size_t block_count, list_size, diff, blkidx = 0;
	sqfs_u64 off, *locations;
	sqfs_meta_writer_t *m;
	int ret;

	block_count = table_size / SQFS_META_BLOCK_SIZE;
	if ((table_size % SQFS_META_BLOCK_SIZE) != 0)
		++block_count;

	locations = alloc_array(sizeof(sqfs_u64), block_count);

	if (locations == NULL)
		return SQFS_ERROR_ALLOC;

	/* Write actual data */
	m = sqfs_meta_writer_create(file, cmp, 0);
	if (m == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto out_idx;
	}

	while (table_size > 0) {
		locations[blkidx++] = htole64(file->get_size(file));

		diff = SQFS_META_BLOCK_SIZE;
		if (diff > table_size)
			diff = table_size;

		ret = sqfs_meta_writer_append(m, data, diff);
		if (ret)
			goto out;

		data = (const char *)data + diff;
		table_size -= diff;
	}

	ret = sqfs_meta_writer_flush(m);
	if (ret)
		goto out;

	/* write location list */
	*start = file->get_size(file);

	list_size = sizeof(sqfs_u64) * block_count;

	off = file->get_size(file);

	ret = file->write_at(file, off, locations, list_size);
	if (ret)
		goto out;

	/* cleanup */
	ret = 0;
out:
	sqfs_drop(m);
out_idx:
	free(locations);
	return ret;
}
