/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#include <stdlib.h>

typedef struct{
	sqfs_ostream_t base;

	sqfs_block_processor_t *proc;
	char filename[];
} block_processor_ostream_t;

static int stream_append(sqfs_ostream_t *base, const void *data, size_t size)
{
	block_processor_ostream_t *strm = (block_processor_ostream_t *)base;

	if (strm->proc == NULL)
		return SQFS_ERROR_SEQUENCE;

	return sqfs_block_processor_append(strm->proc, data, size);
}

static int stream_flush(sqfs_ostream_t *base)
{
	block_processor_ostream_t *strm = (block_processor_ostream_t *)base;
	int ret;

	if (strm->proc == NULL)
		return SQFS_ERROR_SEQUENCE;

	ret = sqfs_block_processor_end_file(strm->proc);
	strm->proc = sqfs_drop(strm->proc);

	return ret;
}

static const char *stream_get_filename(sqfs_ostream_t *strm)
{
	return ((block_processor_ostream_t *)strm)->filename;
}

static void stream_destroy(sqfs_object_t *base)
{
	block_processor_ostream_t *strm = (block_processor_ostream_t *)base;

	if (strm->proc != NULL) {
		sqfs_block_processor_end_file(strm->proc);
		sqfs_drop(strm->proc);
	}

	free(strm);
}

int sqfs_block_processor_create_ostream(sqfs_ostream_t **out,
					const char *filename,
					sqfs_block_processor_t *proc,
					sqfs_inode_generic_t **inode,
					sqfs_u32 flags)
{
	block_processor_ostream_t *strm;
	sqfs_ostream_t *base;
	size_t size, namelen;
	int ret;

	*out = NULL;

	namelen = strlen(filename);
	size = sizeof(*strm) + 1;
	if (SZ_ADD_OV(namelen, size, &size))
		return SQFS_ERROR_ALLOC;

	strm = calloc(1, size);
	base = (sqfs_ostream_t *)strm;
	if (strm == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(strm, stream_destroy, NULL);
	memcpy(strm->filename, filename, namelen);

	ret = sqfs_block_processor_begin_file(proc, inode, NULL, flags);
	if (ret != 0) {
		free(strm);
		return ret;
	}

	strm->proc = sqfs_grab(proc);
	base->append = stream_append;
	base->flush = stream_flush;
	base->get_filename = stream_get_filename;

	*out = base;
	return 0;
}
