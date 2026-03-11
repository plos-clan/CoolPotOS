/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XFRM_COMPRESS_H
#define XFRM_COMPRESS_H

#include "xfrm/stream.h"

typedef struct {
	uint32_t flags;
	uint32_t level;

	union {
		struct {
			uint8_t vli;
			uint8_t pad0[15];
		} xz;

		struct {
			uint16_t window_size;
			uint16_t padd0[7];
		} gzip;

		struct {
			uint8_t work_factor;
			uint8_t padd0[15];
		} bzip2;

		uint64_t padd0[2];
	} opt;
} compressor_config_t;

typedef enum {
	COMP_FLAG_XZ_EXTREME = 0x0001,
} COMP_FLAG_XZ;

typedef enum {
	COMP_XZ_VLI_X86 = 1,
	COMP_XZ_VLI_POWERPC = 2,
	COMP_XZ_VLI_IA64 = 3,
	COMP_XZ_VLI_ARM = 4,
	COMP_XZ_VLI_ARMTHUMB = 5,
	COMP_XZ_VLI_SPARC = 6,
} COMP_XZ_VLI;

#define COMP_GZIP_MIN_LEVEL (1)
#define COMP_GZIP_MAX_LEVEL (9)
#define COMP_GZIP_DEFAULT_LEVEL (9)

#define COMP_GZIP_MIN_WINDOW (8)
#define COMP_GZIP_MAX_WINDOW (15)
#define COMP_GZIP_DEFAULT_WINDOW (15)

#define COMP_ZSTD_MIN_LEVEL (1)
#define COMP_ZSTD_MAX_LEVEL (22)
#define COMP_ZSTD_DEFAULT_LEVEL (15)

#define COMP_BZIP2_MIN_LEVEL (1)
#define COMP_BZIP2_MAX_LEVEL (9)
#define COMP_BZIP2_DEFAULT_LEVEL (9)

#define COMP_BZIP2_MIN_WORK_FACTOR (0)
#define COMP_BZIP2_MAX_WORK_FACTOR (250)
#define COMP_BZIP2_DEFAULT_WORK_FACTOR (30)

#define COMP_XZ_MIN_LEVEL (0)
#define COMP_XZ_MAX_LEVEL (9)
#define COMP_XZ_DEFAULT_LEVEL (6)

enum {
	XFRM_COMPRESSOR_GZIP = 1,
	XFRM_COMPRESSOR_XZ = 2,
	XFRM_COMPRESSOR_ZSTD = 3,
	XFRM_COMPRESSOR_BZIP2 = 4,

	XFRM_COMPRESSOR_MIN = 1,
	XFRM_COMPRESSOR_MAX = 4,
};

#ifdef __cplusplus
extern "C" {
#endif

xfrm_stream_t *compressor_stream_bzip2_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_bzip2_create(void);

xfrm_stream_t *compressor_stream_xz_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_xz_create(void);

xfrm_stream_t *compressor_stream_gzip_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_gzip_create(void);

xfrm_stream_t *compressor_stream_zstd_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_zstd_create(void);

int xfrm_compressor_id_from_name(const char *name);

int xfrm_compressor_id_from_magic(const void *data, size_t count);

const char *xfrm_compressor_name_from_id(int id);

xfrm_stream_t *compressor_stream_create(int id, const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_create(int id);

#ifdef __cplusplus
}
#endif

#endif /* XFRM_COMPRESS_H */
