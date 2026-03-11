/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * zstd.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <zstd.h>
#include <zstd_errors.h>

#include "internal.h"

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
	ZSTD_CCtx *zctx;
	int level;
} zstd_compressor_t;

typedef struct {
	sqfs_u32 level;
} zstd_options_t;

static int zstd_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	zstd_options_t opt;

	if (zstd->level == SQFS_ZSTD_DEFAULT_LEVEL)
		return 0;

	opt.level = htole32(zstd->level);
	return sqfs_generic_write_options(file, &opt, sizeof(opt));
}

static int zstd_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	zstd_options_t opt;
	int ret;
	(void)base;

	ret = sqfs_generic_read_options(file, &opt, sizeof(opt));
	if (ret)
		return ret;

	opt.level = le32toh(opt.level);
	return 0;
}

static sqfs_s32 zstd_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	size_t ret;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	ret = ZSTD_compressCCtx(zstd->zctx, out, outsize, in, size,
				zstd->level);

	if (ZSTD_isError(ret)) {
		if (ZSTD_getErrorCode(ret) == ZSTD_error_dstSize_tooSmall)
			return 0;

		return SQFS_ERROR_COMPRESSOR;
	}

	return ret < size ? ret : 0;
}

static sqfs_s32 zstd_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				  sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	size_t ret;
	(void)base;

	if (outsize >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	ret = ZSTD_decompress(out, outsize, in, size);

	if (ZSTD_isError(ret))
		return SQFS_ERROR_COMPRESSOR;

	return ret;
}

static void zstd_get_configuration(const sqfs_compressor_t *base,
				   sqfs_compressor_config_t *cfg)
{
	const zstd_compressor_t *zstd = (const zstd_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_ZSTD;

	cfg->block_size = zstd->block_size;
	cfg->level = zstd->level;

	if (base->do_block == zstd_uncomp_block)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static sqfs_object_t *zstd_create_copy(const sqfs_object_t *cmp)
{
	zstd_compressor_t *zstd = malloc(sizeof(*zstd));

	if (zstd == NULL)
		return NULL;

	memcpy(zstd, cmp, sizeof(*zstd));

	zstd->zctx = ZSTD_createCCtx();

	if (zstd->zctx == NULL) {
		free(zstd);
		return NULL;
	}

	return (sqfs_object_t *)zstd;
}

static void zstd_destroy(sqfs_object_t *base)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;

	ZSTD_freeCCtx(zstd->zctx);
	free(zstd);
}

int zstd_compressor_create(const sqfs_compressor_config_t *cfg,
			   sqfs_compressor_t **out)
{
	zstd_compressor_t *zstd;
	sqfs_compressor_t *base;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->level < 1 || cfg->level > (unsigned int)ZSTD_maxCLevel())
		return SQFS_ERROR_UNSUPPORTED;

	zstd = calloc(1, sizeof(*zstd));
	base = (sqfs_compressor_t *)zstd;
	if (zstd == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(zstd, zstd_destroy, zstd_create_copy);

	zstd->block_size = cfg->block_size;
	zstd->level = cfg->level;
	zstd->zctx = ZSTD_createCCtx();
	if (zstd->zctx == NULL) {
		free(zstd);
		return SQFS_ERROR_COMPRESSOR;
	}

	base->get_configuration = zstd_get_configuration;
	base->do_block = cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS ?
		zstd_uncomp_block : zstd_comp_block;
	base->write_options = zstd_write_options;
	base->read_options = zstd_read_options;

	*out = base;
	return 0;
}
