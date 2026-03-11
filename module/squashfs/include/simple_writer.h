/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * simple_writer.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SIMPLE_WRITER_H
#define SIMPLE_WRITER_H

#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/block_writer.h"
#include "sqfs/xattr_writer.h"
#include "sqfs/meta_writer.h"
#include "sqfs/frag_table.h"
#include "sqfs/dir_writer.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#include "fstree.h"

typedef struct {
	const char *filename;
	sqfs_block_writer_t *blkwr;
	sqfs_frag_table_t *fragtbl;
	sqfs_block_processor_t *data;
	sqfs_dir_writer_t *dirwr;
	sqfs_meta_writer_t *dm;
	sqfs_meta_writer_t *im;
	sqfs_compressor_t *cmp;
	sqfs_compressor_t *uncmp;
	sqfs_id_table_t *idtbl;
	sqfs_file_t *outfile;
	sqfs_super_t super;
	fstree_t fs;
	sqfs_xattr_writer_t *xwr;
} sqfs_writer_t;

typedef struct {
	const char *filename;
	char *fs_defaults;
	char *comp_extra;
	size_t block_size;
	size_t devblksize;
	size_t max_backlog;
	size_t num_jobs;

	int outmode;
	SQFS_COMPRESSOR comp_id;

	bool exportable;
	bool no_xattr;
	bool quiet;
} sqfs_writer_cfg_t;

#ifdef __cplusplus
extern "C" {
#endif

void sqfs_writer_cfg_init(sqfs_writer_cfg_t *cfg);

int sqfs_writer_init(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *wrcfg);

int sqfs_writer_finish(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *cfg);

void sqfs_writer_cleanup(sqfs_writer_t *sqfs, int status);

/*
  High level helper function to serialize an entire file system tree to
  a squashfs inode table and directory table. The super block is update
  accordingly.

  The function internally creates two meta data writers and uses
  meta_writer_write_inode to serialize the inode table of the fstree.

  Returns 0 on success. Prints error messages to stderr on failure.
  The filename is used to prefix error messages.
 */
int sqfs_serialize_fstree(const char *filename, sqfs_writer_t *wr);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLE_WRITER_H */
