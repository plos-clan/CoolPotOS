/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * lzma.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

#include "internal.h"

#define LZMA_SIZE_OFFSET (5)
#define LZMA_SIZE_BYTES (8)
#define LZMA_HEADER_SIZE (13)

#define MEMLIMIT (64 * 1024 * 1024)

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
	size_t dict_size;

	sqfs_u32 flags;
	sqfs_u8 level;
	sqfs_u8 lc;
	sqfs_u8 lp;
	sqfs_u8 pb;
} lzma_compressor_t;

static int lzma_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	(void)base; (void)file;
	return 0;
}

static int lzma_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	(void)base;
	(void)file;
	return SQFS_ERROR_UNSUPPORTED;
}

static sqfs_s32 try_compress(lzma_compressor_t *lzma, sqfs_u32 preset,
			     const sqfs_u8 *in, size_t size,
			     sqfs_u8 *out, size_t outsize)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_options_lzma opt;
	int ret;

	lzma_lzma_preset(&opt, preset);
	opt.dict_size = lzma->block_size;
	opt.lc = lzma->lc;
	opt.lp = lzma->lp;
	opt.pb = lzma->pb;

	if (lzma_alone_encoder(&strm, &opt) != LZMA_OK) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	strm.next_out = out;
	strm.avail_out = outsize;
	strm.next_in = in;
	strm.avail_in = size;

	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if (ret != LZMA_STREAM_END)
		return ret == LZMA_OK ? 0 : SQFS_ERROR_COMPRESSOR;

	if (strm.total_out > size)
		return 0;

	out[LZMA_SIZE_OFFSET    ] = size & 0xFF;
	out[LZMA_SIZE_OFFSET + 1] = (size >> 8) & 0xFF;
	out[LZMA_SIZE_OFFSET + 2] = (size >> 16) & 0xFF;
	out[LZMA_SIZE_OFFSET + 3] = (size >> 24) & 0xFF;
	out[LZMA_SIZE_OFFSET + 4] = 0;
	out[LZMA_SIZE_OFFSET + 5] = 0;
	out[LZMA_SIZE_OFFSET + 6] = 0;
	out[LZMA_SIZE_OFFSET + 7] = 0;
	return strm.total_out;
}

static sqfs_s32 lzma_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	lzma_compressor_t *lzma = (lzma_compressor_t *)base;
	sqfs_s32 ret, smallest;
	sqfs_u32 preset;

	if (outsize < LZMA_HEADER_SIZE || size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	preset = lzma->level;
	ret = try_compress(lzma, preset, in, size, out, outsize);
	if (ret < 0 || !(lzma->flags & SQFS_COMP_FLAG_LZMA_EXTREME))
		return ret;

	preset |= LZMA_PRESET_EXTREME;
	smallest = ret;

	ret = try_compress(lzma, preset, in, size, out, outsize);
	if (ret < 0 || (ret > 0 && (smallest == 0 || ret < smallest)))
		return ret;

	preset &= ~LZMA_PRESET_EXTREME;
	return smallest == 0 ? 0 :
		try_compress(lzma, preset, in, size, out, outsize);
}

static sqfs_s32 lzma_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				  sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	sqfs_u8 lzma_header[LZMA_HEADER_SIZE];
	lzma_stream strm = LZMA_STREAM_INIT;
	size_t hdrsize;
	int ret;
	(void)base;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	if (size < sizeof(lzma_header))
		return SQFS_ERROR_CORRUPTED;

	hdrsize = (size_t)in[LZMA_SIZE_OFFSET] |
		((size_t)in[LZMA_SIZE_OFFSET + 1] << 8) |
		((size_t)in[LZMA_SIZE_OFFSET + 2] << 16) |
		((size_t)in[LZMA_SIZE_OFFSET + 3] << 24);

	if (hdrsize > outsize)
		return 0;

	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	memcpy(lzma_header, in, sizeof(lzma_header));
	memset(lzma_header + LZMA_SIZE_OFFSET, 0xFF, LZMA_SIZE_BYTES);

	strm.next_out = out;
	strm.avail_out = outsize;
	strm.next_in = lzma_header;
	strm.avail_in = sizeof(lzma_header);

	ret = lzma_code(&strm, LZMA_RUN);

	if (ret != LZMA_OK || strm.avail_in != 0) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	strm.next_in = in + sizeof(lzma_header);
	strm.avail_in = size - sizeof(lzma_header);

	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if (ret != LZMA_STREAM_END && ret != LZMA_OK)
		return SQFS_ERROR_COMPRESSOR;

	if (ret == LZMA_OK) {
		if (strm.total_out < hdrsize || strm.avail_in != 0)
			return 0;
	}

	return hdrsize;
}

static void lzma_get_configuration(const sqfs_compressor_t *base,
				   sqfs_compressor_config_t *cfg)
{
	const lzma_compressor_t *lzma = (const lzma_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_LZMA;
	cfg->block_size = lzma->block_size;
	cfg->flags = lzma->flags;
	cfg->level = lzma->level;
	cfg->opt.lzma.dict_size = lzma->dict_size;
	cfg->opt.lzma.lc = lzma->lc;
	cfg->opt.lzma.lp = lzma->lp;
	cfg->opt.lzma.pb = lzma->pb;
}

static sqfs_object_t *lzma_create_copy(const sqfs_object_t *cmp)
{
	lzma_compressor_t *copy = malloc(sizeof(*copy));

	if (copy != NULL)
		memcpy(copy, cmp, sizeof(*copy));

	return (sqfs_object_t *)copy;
}

static void lzma_destroy(sqfs_object_t *base)
{
	free(base);
}

int lzma_compressor_create(const sqfs_compressor_config_t *cfg,
			   sqfs_compressor_t **out)
{
	sqfs_compressor_t *base;
	lzma_compressor_t *lzma;
	sqfs_u32 mask;

	mask = SQFS_COMP_FLAG_GENERIC_ALL | SQFS_COMP_FLAG_LZMA_ALL;

	if (cfg->flags & ~mask)
		return SQFS_ERROR_UNSUPPORTED;

	/* XXX: values are unsigned and minimum is 0 */
	if (cfg->level > SQFS_LZMA_MAX_LEVEL)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.lc > SQFS_LZMA_MAX_LC)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.lp > SQFS_LZMA_MAX_LP)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.pb > SQFS_LZMA_MAX_PB)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.lc + cfg->opt.lzma.lp > 4)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.dict_size == 0)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.dict_size < SQFS_LZMA_MIN_DICT_SIZE)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzma.dict_size > SQFS_LZMA_MAX_DICT_SIZE)
		return SQFS_ERROR_UNSUPPORTED;

	mask = cfg->opt.lzma.dict_size;
	mask &= mask - 1;

	if (mask != 0) {
		if ((mask & (mask - 1)) != 0)
			return SQFS_ERROR_UNSUPPORTED;

		if (cfg->opt.lzma.dict_size != (mask | mask >> 1))
			return SQFS_ERROR_UNSUPPORTED;
	}

	lzma = calloc(1, sizeof(*lzma));
	base = (sqfs_compressor_t *)lzma;
	if (lzma == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(lzma, lzma_destroy, lzma_create_copy);

	lzma->block_size = cfg->block_size;
	lzma->flags = cfg->flags;
	lzma->level = cfg->level;
	lzma->dict_size = cfg->opt.lzma.dict_size;
	lzma->lc = cfg->opt.lzma.lc;
	lzma->lp = cfg->opt.lzma.lp;
	lzma->pb = cfg->opt.lzma.pb;

	base->get_configuration = lzma_get_configuration;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lzma_uncomp_block : lzma_comp_block;
	base->write_options = lzma_write_options;
	base->read_options = lzma_read_options;

	*out = base;
	return 0;
}
