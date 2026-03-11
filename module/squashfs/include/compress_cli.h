/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress_cli.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMPRESS_CLI_H
#define COMPRESS_CLI_H

#include "sqfs/compressor.h"
#include "sqfs/super.h"
#include "sqfs/block.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#ifdef __cplusplus
extern "C" {
#endif

void compressor_print_available(void);

SQFS_COMPRESSOR compressor_get_default(void);

int compressor_cfg_init_options(sqfs_compressor_config_t *cfg,
				SQFS_COMPRESSOR id,
				size_t block_size, char *options);

void compressor_print_help(SQFS_COMPRESSOR id);

/*
  Create an liblzo2 based LZO compressor.

  XXX: This must be in libcommon instead of libsquashfs for legal reasons.
 */
int lzo_compressor_create(const sqfs_compressor_config_t *cfg,
			  sqfs_compressor_t **out);

#ifdef __cplusplus
}
#endif

#endif /* COMPRESS_CLI_H */
