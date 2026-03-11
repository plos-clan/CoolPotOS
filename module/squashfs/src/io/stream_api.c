/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * stream_api.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"

#include <string.h>

sqfs_s32 sqfs_istream_read(sqfs_istream_t *strm, void *data, size_t size)
{
	sqfs_s32 total = 0;

	if (size > 0x7FFFFFFF)
		size = 0x7FFFFFFF;

	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = strm->get_buffered_data(strm, &ptr, &diff, size);
		if (ret > 0)
			break;
		if (ret < 0)
			return ret;

		if (diff > size)
			diff = size;

		memcpy(data, ptr, diff);
		strm->advance_buffer(strm, diff);
		data = (char *)data + diff;
		size -= diff;
		total += diff;
	}

	return total;
}

int sqfs_istream_skip(sqfs_istream_t *strm, sqfs_u64 size)
{
	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = strm->get_buffered_data(strm, &ptr, &diff, size);
		if (ret < 0)
			return ret;
		if (ret > 0)
			break;

		if ((sqfs_u64)diff > size)
			diff = size;

		size -= diff;
		strm->advance_buffer(strm, diff);
	}

	return 0;
}

sqfs_s32 sqfs_istream_splice(sqfs_istream_t *in, sqfs_ostream_t *out,
			     sqfs_u32 size)
{
	sqfs_s32 total = 0;

	if (size > 0x7FFFFFFF)
		size = 0x7FFFFFFF;

	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = in->get_buffered_data(in, &ptr, &diff, size);
		if (ret < 0)
			return ret;
		if (ret > 0)
			break;

		if (diff > size)
			diff = size;

		ret = out->append(out, ptr, diff);
		if (ret)
			return ret;

		total += diff;
		size -= diff;
		in->advance_buffer(in, diff);
	}

	return total;
}
