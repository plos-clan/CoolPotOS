/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xz.c
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

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
	size_t dict_size;

	sqfs_u8 level;
	sqfs_u8 lc;
	sqfs_u8 lp;
	sqfs_u8 pb;

	int flags;
} xz_compressor_t;

typedef struct {
	sqfs_u32 dict_size;
	sqfs_u32 flags;
} xz_options_t;

static bool is_dict_size_valid(size_t size)
{
	size_t x = size & (size - 1);

	if (x == 0)
		return true;

	return size == (x | (x >> 1));
}

static int xz_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;
	sqfs_u32 flags;

	if (xz->flags == 0 && xz->dict_size == xz->block_size)
		return 0;

	flags = xz->flags & SQFS_COMP_FLAG_XZ_ALL;
	flags &= ~SQFS_COMP_FLAG_XZ_EXTREME;

	opt.dict_size = htole32(xz->dict_size);
	opt.flags = htole32(flags);

	return sqfs_generic_write_options(file, &opt, sizeof(opt));
}

static int xz_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;
	int ret;

	ret = sqfs_generic_read_options(file, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt.dict_size = le32toh(opt.dict_size);
	opt.flags = le32toh(opt.flags);

	if (!is_dict_size_valid(opt.dict_size))
		return SQFS_ERROR_CORRUPTED;

	if (opt.flags & ~SQFS_COMP_FLAG_XZ_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	xz->flags = opt.flags;
	xz->dict_size = opt.dict_size;
	return 0;
}

static sqfs_s32 compress(xz_compressor_t *xz, lzma_vli filter,
			 const sqfs_u8 *in, sqfs_u32 size,
			 sqfs_u8 *out, sqfs_u32 outsize,
			 sqfs_u32 presets)
{
	lzma_filter filters[5];
	lzma_options_lzma opt;
	size_t written = 0;
	lzma_ret ret;
	int i = 0;

	if (lzma_lzma_preset(&opt, presets))
		return SQFS_ERROR_COMPRESSOR;

	opt.lc = xz->lc;
	opt.lp = xz->lp;
	opt.pb = xz->pb;
	opt.dict_size = xz->dict_size;

	if (filter != LZMA_VLI_UNKNOWN) {
		filters[i].id = filter;
		filters[i].options = NULL;
		++i;
	}

	filters[i].id = LZMA_FILTER_LZMA2;
	filters[i].options = &opt;
	++i;

	filters[i].id = LZMA_VLI_UNKNOWN;
	filters[i].options = NULL;
	++i;

	ret = lzma_stream_buffer_encode(filters, LZMA_CHECK_CRC32, NULL,
					in, size, out, &written, outsize);

	if (ret == LZMA_OK)
		return (written >= size) ? 0 : written;

	if (ret != LZMA_BUF_ERROR)
		return SQFS_ERROR_COMPRESSOR;

	return 0;
}

static lzma_vli flag_to_vli(int flag)
{
	switch (flag) {
	case SQFS_COMP_FLAG_XZ_X86:
		return LZMA_FILTER_X86;
	case SQFS_COMP_FLAG_XZ_POWERPC:
		return LZMA_FILTER_POWERPC;
	case SQFS_COMP_FLAG_XZ_IA64:
		return LZMA_FILTER_IA64;
	case SQFS_COMP_FLAG_XZ_ARM:
		return LZMA_FILTER_ARM;
	case SQFS_COMP_FLAG_XZ_ARMTHUMB:
		return LZMA_FILTER_ARMTHUMB;
	case SQFS_COMP_FLAG_XZ_SPARC:
		return LZMA_FILTER_SPARC;
	default:
		break;
	}

	return LZMA_VLI_UNKNOWN;
}

static sqfs_s32 xz_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
			      sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	lzma_vli filter, selected = LZMA_VLI_UNKNOWN;
	sqfs_s32 ret, smallest;
	bool extreme;
	size_t i;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	ret = compress(xz, LZMA_VLI_UNKNOWN, in, size, out,
		       outsize, xz->level);
	if (ret < 0 || xz->flags == 0)
		return ret;

	smallest = ret;
	extreme = false;

	if (xz->flags & SQFS_COMP_FLAG_XZ_EXTREME) {
		ret = compress(xz, LZMA_VLI_UNKNOWN, in, size, out, outsize,
			       xz->level | LZMA_PRESET_EXTREME);

		if (ret > 0 && (smallest == 0 || ret < smallest)) {
			smallest = ret;
			extreme = true;
		}
	}

	for (i = 1; i & SQFS_COMP_FLAG_XZ_ALL; i <<= 1) {
		if ((i & SQFS_COMP_FLAG_XZ_EXTREME) || (xz->flags & i) == 0)
			continue;

		filter = flag_to_vli(i);

		ret = compress(xz, filter, in, size, out, outsize, xz->level);
		if (ret > 0 && (smallest == 0 || ret < smallest)) {
			smallest = ret;
			selected = filter;
			extreme = false;
		}

		if (xz->flags & SQFS_COMP_FLAG_XZ_EXTREME) {
			ret = compress(xz, filter, in, size, out, outsize,
				       xz->level | LZMA_PRESET_EXTREME);

			if (ret > 0 && (smallest == 0 || ret < smallest)) {
				smallest = ret;
				selected = filter;
				extreme = true;
			}
		}
	}

	if (smallest == 0)
		return 0;

	return compress(xz, selected, in, size, out, outsize,
			xz->level | (extreme ? LZMA_PRESET_EXTREME : 0));
}

static sqfs_s32 xz_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	sqfs_u64 memlimit = 65 * 1024 * 1024;
	size_t dest_pos = 0;
	size_t src_pos = 0;
	lzma_ret ret;
	(void)base;

	if (outsize >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	ret = lzma_stream_buffer_decode(&memlimit, 0, NULL,
					in, &src_pos, size,
					out, &dest_pos, outsize);

	if (ret == LZMA_OK && size == src_pos)
		return dest_pos;

	return SQFS_ERROR_COMPRESSOR;
}

static void xz_get_configuration(const sqfs_compressor_t *base,
				 sqfs_compressor_config_t *cfg)
{
	const xz_compressor_t *xz = (const xz_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_XZ;
	cfg->flags = xz->flags;
	cfg->block_size = xz->block_size;
	cfg->level = xz->level;
	cfg->opt.xz.dict_size = xz->dict_size;
	cfg->opt.xz.lc = xz->lc;
	cfg->opt.xz.lp = xz->lp;
	cfg->opt.xz.pb = xz->pb;

	if (base->do_block == xz_uncomp_block)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static sqfs_object_t *xz_create_copy(const sqfs_object_t *cmp)
{
	xz_compressor_t *xz = malloc(sizeof(*xz));

	if (xz == NULL)
		return NULL;

	memcpy(xz, cmp, sizeof(*xz));
	return (sqfs_object_t *)xz;
}

static void xz_destroy(sqfs_object_t *base)
{
	free(base);
}

int xz_compressor_create(const sqfs_compressor_config_t *cfg,
			 sqfs_compressor_t **out)
{
	sqfs_compressor_t *base;
	xz_compressor_t *xz;

	if (cfg->flags & ~(SQFS_COMP_FLAG_GENERIC_ALL |
			   SQFS_COMP_FLAG_XZ_ALL)) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (!is_dict_size_valid(cfg->opt.xz.dict_size))
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.xz.lc + cfg->opt.xz.lp > 4)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.xz.pb > SQFS_XZ_MAX_PB)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->level > SQFS_XZ_MAX_LEVEL)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.xz.dict_size < SQFS_XZ_MIN_DICT_SIZE)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.xz.dict_size > SQFS_XZ_MAX_DICT_SIZE)
		return SQFS_ERROR_UNSUPPORTED;

	xz = calloc(1, sizeof(*xz));
	base = (sqfs_compressor_t *)xz;
	if (xz == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(xz, xz_destroy, xz_create_copy);

	xz->flags = cfg->flags;
	xz->dict_size = cfg->opt.xz.dict_size;
	xz->block_size = cfg->block_size;
	xz->lc = cfg->opt.xz.lc;
	xz->lp = cfg->opt.xz.lp;
	xz->pb = cfg->opt.xz.pb;
	xz->level = cfg->level;
	base->get_configuration = xz_get_configuration;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		xz_uncomp_block : xz_comp_block;
	base->write_options = xz_write_options;
	base->read_options = xz_read_options;

	*out = base;
	return 0;
}
