/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_DIR_ITERATOR_H
#define IO_DIR_ITERATOR_H

#include "sqfs/dir_entry.h"
#include "sqfs/predef.h"

enum {
	DIR_SCAN_NO_SOCK = 0x0001,
	DIR_SCAN_NO_SLINK = 0x0002,
	DIR_SCAN_NO_FILE = 0x0004,
	DIR_SCAN_NO_BLK = 0x0008,
	DIR_SCAN_NO_DIR = 0x0010,
	DIR_SCAN_NO_CHR = 0x0020,
	DIR_SCAN_NO_FIFO = 0x0040,

	DIR_SCAN_KEEP_TIME = 0x0100,
	DIR_SCAN_KEEP_UID = 0x0200,
	DIR_SCAN_KEEP_GID = 0x0400,
	DIR_SCAN_KEEP_MODE = 0x0800,

	DIR_SCAN_ONE_FILESYSTEM = 0x1000,
	DIR_SCAN_NO_RECURSION = 0x2000,
	DIR_SCAN_NO_HARDLINKS = 0x4000,
	DIR_SCAN_MATCH_FULL_PATH = 0x8000,
};

typedef struct {
	sqfs_u32 flags;
	sqfs_u32 def_uid;
	sqfs_u32 def_gid;
	sqfs_u32 def_mode;
	sqfs_s64 def_mtime;

	/**
	 * @brief A prefix to attach to all returend paths
	 *
	 * If not null, this string and an additional "/" are prepended to
	 * all entries returned by the iterator.
	 */
	const char *prefix;

	/**
	 * @brief A glob pattern that either the name must match
	 *
	 * If this is not NULL, only paths that match this globbing pattern
	 * are returned. If the flag DIR_SCAN_MATCH_FULL_PATH is set, the
	 * entire path must match, slashes cannot match wild card characters.
	 * If not set, only the last part of the path is tested. The iterator
	 * still recurses into directories, it simply doesn't report them if
	 * they don't match.
	 */
	const char *name_pattern;
} dir_tree_cfg_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a stacked, recursive directory tree iterator
 *
 * The underlying implementation automatically recurses into sub directories
 * and returns a flattened list of entries, where each entry represents a full
 * path. Advanced filtering, path pre-fixing et cetera can be configured.
 * The typical "." and ".." entries are filtered out as well.
 *
 * @param path A path to a directory on the file system.
 * @param cfg A @ref dir_tree_cfg_t filtering configuration.
 *
 * @return A pointer to a sqfs_dir_iterator_t implementation on success,
 *         NULL on error (message is printed to stderr).
 */
SQFS_INTERNAL
sqfs_dir_iterator_t *dir_tree_iterator_create(const char *path,
					      const dir_tree_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* IO_DIR_ITERATOR_H */
