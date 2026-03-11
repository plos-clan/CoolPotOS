/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * gzip.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>

#include "internal.h"

typedef struct {
	sqfs_u32 level;
	sqfs_u16 window;
	sqfs_u16 strategies;
} gzip_options_t;

typedef struct {
	sqfs_compressor_t base;

	z_stream strm;
	bool compress;

	size_t block_size;
	gzip_options_t opt;
} gzip_compressor_t;

static void gzip_destroy(sqfs_object_t *base)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;

	if (gzip->compress) {
		deflateEnd(&gzip->strm);
	} else {
		inflateEnd(&gzip->strm);
	}

	free(gzip);
}

static void gzip_get_configuration(const sqfs_compressor_t *base,
				   sqfs_compressor_config_t *cfg)
{
	const gzip_compressor_t *gzip = (const gzip_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_GZIP;
	cfg->flags = gzip->opt.strategies;
	cfg->block_size = gzip->block_size;
	cfg->level = gzip->opt.level;
	cfg->opt.gzip.window_size = gzip->opt.window;

	if (!gzip->compress)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static int gzip_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	gzip_options_t opt;

	if (gzip->opt.level == SQFS_GZIP_DEFAULT_LEVEL &&
	    gzip->opt.window == SQFS_GZIP_DEFAULT_WINDOW &&
	    gzip->opt.strategies == 0) {
		return 0;
	}

	opt.level = htole32(gzip->opt.level);
	opt.window = htole16(gzip->opt.window);
	opt.strategies = htole16(gzip->opt.strategies);

	return sqfs_generic_write_options(file, &opt, sizeof(opt));
}

static int gzip_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	gzip_options_t opt;
	int ret;

	ret = sqfs_generic_read_options(file, &opt, sizeof(opt));
	if (ret)
		return ret;

	gzip->opt.level = le32toh(opt.level);
	gzip->opt.window = le16toh(opt.window);
	gzip->opt.strategies = le16toh(opt.strategies);

	if (gzip->opt.level < 1 || gzip->opt.level > 9)
		return SQFS_ERROR_UNSUPPORTED;

	if (gzip->opt.window < 8 || gzip->opt.window > 15)
		return SQFS_ERROR_UNSUPPORTED;

	if (gzip->opt.strategies & ~SQFS_COMP_FLAG_GZIP_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	return 0;
}

static int flag_to_zlib_strategy(int flag)
{
	switch (flag) {
	case SQFS_COMP_FLAG_GZIP_DEFAULT:
		return Z_DEFAULT_STRATEGY;
	case SQFS_COMP_FLAG_GZIP_FILTERED:
		return Z_FILTERED;
	case SQFS_COMP_FLAG_GZIP_HUFFMAN:
		return Z_HUFFMAN_ONLY;
	case SQFS_COMP_FLAG_GZIP_RLE:
		return Z_RLE;
	case SQFS_COMP_FLAG_GZIP_FIXED:
		return Z_FIXED;
	default:
		break;
	}

	return 0;
}

static int find_strategy(gzip_compressor_t *gzip, const sqfs_u8 *in,
			 sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	int ret, strategy, selected = Z_DEFAULT_STRATEGY;
	size_t i, length, minlength = 0;

	for (i = 0x01; i & SQFS_COMP_FLAG_GZIP_ALL; i <<= 1) {
		if ((gzip->opt.strategies & i) == 0)
			continue;

		ret = deflateReset(&gzip->strm);
		if (ret != Z_OK)
			return SQFS_ERROR_COMPRESSOR;

		strategy = flag_to_zlib_strategy(i);

		gzip->strm.next_in = (z_const Bytef *)in;
		gzip->strm.avail_in = size;
		gzip->strm.next_out = out;
		gzip->strm.avail_out = outsize;

		ret = deflateParams(&gzip->strm, gzip->opt.level, strategy);
		if (ret != Z_OK)
			return SQFS_ERROR_COMPRESSOR;

		ret = deflate(&gzip->strm, Z_FINISH);

		if (ret == Z_STREAM_END) {
			length = gzip->strm.total_out;

			if (minlength == 0 || length < minlength) {
				minlength = length;
				selected = strategy;
			}
		} else if (ret != Z_OK && ret != Z_BUF_ERROR) {
			return SQFS_ERROR_COMPRESSOR;
		}
	}

	return selected;
}

static sqfs_s32 gzip_do_block(sqfs_compressor_t *base, const sqfs_u8 *in,
			      sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	int ret, strategy = 0;
	size_t written;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	if (gzip->compress && gzip->opt.strategies != 0) {
		strategy = find_strategy(gzip, in, size, out, outsize);
		if (strategy < 0)
			return strategy;
	}

	if (gzip->compress) {
		ret = deflateReset(&gzip->strm);
	} else {
		ret = inflateReset(&gzip->strm);
	}

	if (ret != Z_OK)
		return SQFS_ERROR_COMPRESSOR;

	gzip->strm.next_in = (const void *)in;
	gzip->strm.avail_in = size;
	gzip->strm.next_out = out;
	gzip->strm.avail_out = outsize;

	if (gzip->compress && gzip->opt.strategies != 0) {
		ret = deflateParams(&gzip->strm, gzip->opt.level, strategy);
		if (ret != Z_OK)
			return SQFS_ERROR_COMPRESSOR;
	}

	if (gzip->compress) {
		ret = deflate(&gzip->strm, Z_FINISH);
	} else {
		ret = inflate(&gzip->strm, Z_FINISH);
	}

	if (ret == Z_STREAM_END) {
		written = gzip->strm.total_out;

		if (gzip->compress && written >= size)
			return 0;

		return written;
	}

	if (ret != Z_OK && ret != Z_BUF_ERROR)
		return SQFS_ERROR_COMPRESSOR;

	return 0;
}

static sqfs_object_t *gzip_create_copy(const sqfs_object_t *cmp)
{
	gzip_compressor_t *gzip = malloc(sizeof(*gzip));
	int ret;

	if (gzip == NULL)
		return NULL;

	memcpy(gzip, cmp, sizeof(*gzip));
	memset(&gzip->strm, 0, sizeof(gzip->strm));

	if (gzip->compress) {
		ret = deflateInit2(&gzip->strm, gzip->opt.level, Z_DEFLATED,
				   gzip->opt.window, 8, Z_DEFAULT_STRATEGY);
	} else {
		ret = inflateInit(&gzip->strm);
	}

	if (ret != Z_OK) {
		free(gzip);
		return NULL;
	}

	return (sqfs_object_t *)gzip;
}

int gzip_compressor_create(const sqfs_compressor_config_t *cfg,
			   sqfs_compressor_t **out)
{
	gzip_compressor_t *gzip;
	sqfs_compressor_t *base;
	int ret;

	if (cfg->flags & ~(SQFS_COMP_FLAG_GZIP_ALL |
			   SQFS_COMP_FLAG_GENERIC_ALL)) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (cfg->level < SQFS_GZIP_MIN_LEVEL ||
	    cfg->level > SQFS_GZIP_MAX_LEVEL) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (cfg->opt.gzip.window_size < SQFS_GZIP_MIN_WINDOW ||
	    cfg->opt.gzip.window_size > SQFS_GZIP_MAX_WINDOW) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	gzip = calloc(1, sizeof(*gzip));
	base = (sqfs_compressor_t *)gzip;

	if (gzip == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(gzip, gzip_destroy, gzip_create_copy);

	gzip->opt.level = cfg->level;
	gzip->opt.window = cfg->opt.gzip.window_size;
	gzip->opt.strategies = cfg->flags & SQFS_COMP_FLAG_GZIP_ALL;
	gzip->compress = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) == 0;
	gzip->block_size = cfg->block_size;
	base->get_configuration = gzip_get_configuration;
	base->do_block = gzip_do_block;
	base->write_options = gzip_write_options;
	base->read_options = gzip_read_options;

	if (gzip->compress) {
		ret = deflateInit2(&gzip->strm, cfg->level,
				   Z_DEFLATED, cfg->opt.gzip.window_size, 8,
				   Z_DEFAULT_STRATEGY);
	} else {
		ret = inflateInit(&gzip->strm);
	}

	if (ret != Z_OK) {
		free(gzip);
		return SQFS_ERROR_COMPRESSOR;
	}

	*out = base;
	return 0;
}
