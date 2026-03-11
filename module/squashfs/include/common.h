/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMMON_H
#define COMMON_H

#include "config.h"

#include "sqfs/xattr_reader.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/block.h"
#include "sqfs/xattr.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include "simple_writer.h"
#include "compress_cli.h"
#include "dir_tree.h"
#include "compat.h"
#include "fstree.h"

#include <stddef.h>

void sqfs_perror(const char *file, const char *action, int error_code);

/* A common implementation of the '--version' command line flag. */
void print_version(const char *progname);

/*
  Parse a number optionally followed by a KMG suffix (case insensitive). Prints
  an error message to stderr and returns -1 on failure, 0 on success.

  The "what" string is used to prefix error messages (perror style).

  If reference is non-zero, the suffix '%' can be used to compute the result as
  a multiple of the reference value.
 */
int parse_size(const char *what, size_t *out, const char *str,
	       size_t reference);

void print_size(sqfs_u64 size, char *buffer, bool round_to_int);

/*
  Parse a comma separated list (e.g. "uid=...,gid=..." of defaults for
  fstree nodes. Used for command line parsing. Returns 0 on success,
  -1 on failure. Prints an error message to stderr on failure.
 */
int parse_fstree_defaults(fstree_defaults_t *out, char *str);

int istream_open_stdin(sqfs_istream_t **out);

int ostream_open_stdout(sqfs_ostream_t **out);

sqfs_istream_t *istream_memory_create(const char *name, size_t bufsz,
				      const void *data, size_t size);

#endif /* COMMON_H */
