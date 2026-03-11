/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * lz4.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lz4.h>
#include <lz4hc.h>

#include "internal.h"

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
	bool high_compression;
} lz4_compressor_t;

typedef struct {
	sqfs_u32 version;
	sqfs_u32 flags;
} lz4_options;

#define LZ4LEGACY 1

/* old verions of liblz4 don't have this */
#ifndef LZ4HC_CLEVEL_MAX
#define LZ4HC_CLEVEL_MAX 12
#endif

static int lz4_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lz4_compressor_t *lz4 = (lz4_compressor_t *)base;
	lz4_options opt = {
		.version = htole32(LZ4LEGACY),
		.flags = htole32(lz4->high_compression ?
				 SQFS_COMP_FLAG_LZ4_HC : 0),
	};

	return sqfs_generic_write_options(file, &opt, sizeof(opt));
}

static int lz4_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lz4_options opt;
	int ret;
	(void)base;

	ret = sqfs_generic_read_options(file, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt.version = le32toh(opt.version);
	opt.flags = le32toh(opt.flags);

	if (opt.version != LZ4LEGACY)
		return SQFS_ERROR_UNSUPPORTED;

	return 0;
}

static sqfs_s32 lz4_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
			       sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	lz4_compressor_t *lz4 = (lz4_compressor_t *)base;
	int ret;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	if (lz4->high_compression) {
		ret = LZ4_compress_HC((const void *)in, (void *)out,
				      size, outsize, LZ4HC_CLEVEL_MAX);
	} else {
		ret = LZ4_compress_default((const void *)in, (void *)out,
					   size, outsize);
	}

	if (ret < 0)
		return SQFS_ERROR_COMPRESSOR;

	return ret;
}

static sqfs_s32 lz4_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				 sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	int ret;
	(void)base;

	if (outsize >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	ret = LZ4_decompress_safe((const void *)in, (void *)out, size, outsize);

	if (ret < 0)
		return SQFS_ERROR_COMPRESSOR;

	return ret;
}

static void lz4_get_configuration(const sqfs_compressor_t *base,
				  sqfs_compressor_config_t *cfg)
{
	const lz4_compressor_t *lz4 = (const lz4_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_LZ4;
	cfg->block_size = lz4->block_size;

	if (lz4->high_compression)
		cfg->flags |= SQFS_COMP_FLAG_LZ4_HC;

	if (base->do_block == lz4_uncomp_block)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static sqfs_object_t *lz4_create_copy(const sqfs_object_t *cmp)
{
	lz4_compressor_t *lz4 = malloc(sizeof(*lz4));

	if (lz4 == NULL)
		return NULL;

	memcpy(lz4, cmp, sizeof(*lz4));
	return (sqfs_object_t *)lz4;
}

static void lz4_destroy(sqfs_object_t *base)
{
	free(base);
}

int lz4_compressor_create(const sqfs_compressor_config_t *cfg,
			  sqfs_compressor_t **out)
{
	sqfs_compressor_t *base;
	lz4_compressor_t *lz4;

	if (cfg->flags & ~(SQFS_COMP_FLAG_LZ4_ALL |
			   SQFS_COMP_FLAG_GENERIC_ALL)) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (cfg->level != 0)
		return SQFS_ERROR_UNSUPPORTED;

	lz4 = calloc(1, sizeof(*lz4));
	base = (sqfs_compressor_t *)lz4;
	if (lz4 == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(lz4, lz4_destroy, lz4_create_copy);

	lz4->high_compression = (cfg->flags & SQFS_COMP_FLAG_LZ4_HC) != 0;
	lz4->block_size = cfg->block_size;

	base->get_configuration = lz4_get_configuration;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lz4_uncomp_block : lz4_comp_block;
	base->write_options = lz4_write_options;
	base->read_options = lz4_read_options;

	*out = base;
	return 0;
}
