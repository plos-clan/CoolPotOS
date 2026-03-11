/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stream.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XFRM_STREAM_H
#define XFRM_STREAM_H

#include "sqfs/predef.h"

typedef enum {
	XFRM_STREAM_FLUSH_NONE = 0,
	XFRM_STREAM_FLUSH_SYNC,
	XFRM_STREAM_FLUSH_FULL,

	XFRM_STREAM_FLUSH_COUNT,
} XFRM_STREAM_FLUSH;

typedef enum {
	XFRM_STREAM_ERROR = -1,
	XFRM_STREAM_OK = 0,
	XFRM_STREAM_END = 1,
	XFRM_STREAM_BUFFER_FULL = 2,
} XFRM_STREAM_RESULT;

typedef struct xfrm_stream_t xfrm_stream_t;

struct xfrm_stream_t {
	sqfs_object_t base;

	int (*process_data)(xfrm_stream_t *stream,
			    const void *in, sqfs_u32 in_size,
			    void *out, sqfs_u32 out_size,
			    sqfs_u32 *in_read, sqfs_u32 *out_written,
			    int flush_mode);
};

#endif /* XFRM_STREAM_H */
