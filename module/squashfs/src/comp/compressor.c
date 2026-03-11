/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * compressor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "internal.h"

typedef int (*compressor_fun_t)(const sqfs_compressor_config_t *cfg,
				sqfs_compressor_t **out);

static compressor_fun_t compressors[SQFS_COMP_MAX + 1] = {
#ifdef WITH_GZIP
	[SQFS_COMP_GZIP] = gzip_compressor_create,
#endif
#ifdef WITH_XZ
	[SQFS_COMP_XZ] = xz_compressor_create,
	[SQFS_COMP_LZMA] = lzma_compressor_create,
#endif
#ifdef WITH_LZ4
	[SQFS_COMP_LZ4] = lz4_compressor_create,
#endif
#ifdef WITH_ZSTD
	[SQFS_COMP_ZSTD] = zstd_compressor_create,
#endif
};

static const char *names[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
};

int sqfs_generic_write_options(sqfs_file_t *file, const void *data, size_t size)
{
	sqfs_u8 buffer[64];
	sqfs_u16 header;
	int ret;

	/* XXX: options for all known compressors should fit into this */
	if (size >= (sizeof(buffer) - sizeof(header)))
		return SQFS_ERROR_INTERNAL;

	header = htole16(0x8000 | size);
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), data, size);

	ret = file->write_at(file, sizeof(sqfs_super_t),
			     buffer, sizeof(header) + size);
	if (ret)
		return ret;

	return sizeof(header) + size;
}

int sqfs_generic_read_options(sqfs_file_t *file, void *data, size_t size)
{
	sqfs_u8 buffer[64];
	sqfs_u16 header;
	int ret;

	/* XXX: options for all known compressors should fit into this */
	if (size >= (sizeof(buffer) - sizeof(header)))
		return SQFS_ERROR_INTERNAL;

	ret = file->read_at(file, sizeof(sqfs_super_t),
			    buffer, sizeof(header) + size);
	if (ret)
		return ret;

	memcpy(&header, buffer, sizeof(header));

	if (le16toh(header) != (0x8000 | size))
		return SQFS_ERROR_CORRUPTED;

	memcpy(data, buffer + 2, size);
	return 0;
}

int sqfs_compressor_create(const sqfs_compressor_config_t *cfg,
			   sqfs_compressor_t **out)
{
	sqfs_u8 padd0[sizeof(cfg->opt)];
	int ret;

	*out = NULL;

	/* check compressor ID */
	if (cfg == NULL)
		return SQFS_ERROR_ARG_INVALID;

	if (cfg->id < SQFS_COMP_MIN || cfg->id > SQFS_COMP_MAX)
		return SQFS_ERROR_UNSUPPORTED;

	if (compressors[cfg->id] == NULL)
		return SQFS_ERROR_UNSUPPORTED;

	/* make sure the padding bytes are cleared, so we could theoretically
	   turn them into option fields in the future and remain compatible */
	memset(padd0, 0, sizeof(padd0));

	switch (cfg->id) {
	case SQFS_COMP_XZ:
		ret = memcmp(cfg->opt.xz.padd0, padd0,
			     sizeof(cfg->opt.xz.padd0));
		break;
	case SQFS_COMP_LZMA:
		ret = memcmp(cfg->opt.lzma.padd0, padd0,
			     sizeof(cfg->opt.lzma.padd0));
		break;
	case SQFS_COMP_LZO:
		ret = memcmp(cfg->opt.lzo.padd0, padd0,
			     sizeof(cfg->opt.lzo.padd0));
		break;
	case SQFS_COMP_GZIP:
		ret = memcmp(cfg->opt.gzip.padd0, padd0,
			     sizeof(cfg->opt.gzip.padd0));
		break;
	default:
		ret = memcmp(cfg->opt.padd0, padd0, sizeof(cfg->opt.padd0));
		break;
	}

	if (ret != 0)
		return SQFS_ERROR_ARG_INVALID;

	return compressors[cfg->id](cfg, out);
}

const char *sqfs_compressor_name_from_id(SQFS_COMPRESSOR id)
{
	if (id < 0 || (size_t)id >= sizeof(names) / sizeof(names[0]))
		return NULL;

	return names[id];
}

int sqfs_compressor_id_from_name(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (names[i] != NULL && strcmp(names[i], name) == 0)
			return i;
	}

	return SQFS_ERROR_UNSUPPORTED;
}

int sqfs_compressor_config_init(sqfs_compressor_config_t *cfg,
				SQFS_COMPRESSOR id,
				size_t block_size, sqfs_u16 flags)
{
	sqfs_u32 flag_mask = SQFS_COMP_FLAG_GENERIC_ALL;

	memset(cfg, 0, sizeof(*cfg));

	switch (id) {
	case SQFS_COMP_GZIP:
		flag_mask |= SQFS_COMP_FLAG_GZIP_ALL;
		cfg->level = SQFS_GZIP_DEFAULT_LEVEL;
		cfg->opt.gzip.window_size = SQFS_GZIP_DEFAULT_WINDOW;
		break;
	case SQFS_COMP_LZO:
		cfg->opt.lzo.algorithm = SQFS_LZO_DEFAULT_ALG;
		cfg->level = SQFS_LZO_DEFAULT_LEVEL;
		break;
	case SQFS_COMP_ZSTD:
		cfg->level = SQFS_ZSTD_DEFAULT_LEVEL;
		break;
	case SQFS_COMP_XZ:
		flag_mask |= SQFS_COMP_FLAG_XZ_ALL;
		cfg->level = SQFS_XZ_DEFAULT_LEVEL;
		cfg->opt.xz.dict_size = block_size;
		cfg->opt.xz.lc = SQFS_XZ_DEFAULT_LC;
		cfg->opt.xz.lp = SQFS_XZ_DEFAULT_LP;
		cfg->opt.xz.pb = SQFS_XZ_DEFAULT_PB;

		if (block_size < SQFS_XZ_MIN_DICT_SIZE)
			cfg->opt.xz.dict_size = SQFS_XZ_MIN_DICT_SIZE;
		break;
	case SQFS_COMP_LZMA:
		flag_mask |= SQFS_COMP_FLAG_LZMA_ALL;
		cfg->level = SQFS_LZMA_DEFAULT_LEVEL;
		cfg->opt.lzma.dict_size = block_size;
		cfg->opt.lzma.lc = SQFS_LZMA_DEFAULT_LC;
		cfg->opt.lzma.lp = SQFS_LZMA_DEFAULT_LP;
		cfg->opt.lzma.pb = SQFS_LZMA_DEFAULT_PB;

		if (block_size < SQFS_LZMA_MIN_DICT_SIZE)
			cfg->opt.lzma.dict_size = SQFS_LZMA_MIN_DICT_SIZE;
		break;
	case SQFS_COMP_LZ4:
		flag_mask |= SQFS_COMP_FLAG_LZ4_ALL;
		break;
	default:
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (flags & ~flag_mask) {
		memset(cfg, 0, sizeof(*cfg));
		return SQFS_ERROR_UNSUPPORTED;
	}

	cfg->id = id;
	cfg->flags = flags;
	cfg->block_size = block_size;
	return 0;
}
