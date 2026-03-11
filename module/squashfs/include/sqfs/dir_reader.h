/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_reader.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_DIR_READER_H
#define SQFS_DIR_READER_H

#include "sqfs/predef.h"
#include "sqfs/meta_reader.h"

/**
 * @file dir_reader.h
 *
 * @brief Contains declarations for the @ref sqfs_dir_reader_t
 */

/**
 * @struct sqfs_dir_reader_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts reading of directory entries
 *
 * SquashFS stores directory listings and inode structures separated from
 * each other in meta data blocks.
 *
 * The sqfs_dir_reader_t abstracts access to the filesystem tree in a SquashFS
 * through a fairly simple interface. It keeps two meta data readers internally
 * for reading directory listings and inodes. Externally, it offers a few
 * simple functions for iterating over the contents of a directory that
 * completely take care of fetching/decoding headers and sifting through the
 * multi level hierarchie used for storing them on disk.
 *
 * See @ref sqfs_dir_writer_t for an overview on how directory entries are
 * stored in SquashFS.
 *
 * The reader also abstracts easy access to the underlying inodes, allowing
 * direct access to the inode referred to by a directory entry.
 */

/**
 * @enum SQFS_DIR_READER_FLAGS
 *
 * @brief Flags for @ref sqfs_dir_reader_create
 */
typedef enum {
	/**
	 * @brief Support "." and ".." directory and path entries.
	 *
	 * If this flag is set, the directory reader returns "." and ".."
	 * entries when iterating over a directory, can fetch the associated
	 * inodes if requested and supports resolving "." and ".." path
	 * components when looking up a full path.
	 *
	 * In order for this to work, it internally caches the locations of
	 * directory inodes it encounteres. This means, it only works as long
	 * as you only use inodes fetched through the directory reader. If
	 * given a foreign inode it hasn't seen before, it might not be able
	 * to resolve the parent link.
	 */
	SQFS_DIR_READER_DOT_ENTRIES = 0x00000001,

	SQFS_DIR_READER_ALL_FLAGS = 0x00000001,
} SQFS_DIR_READER_FLAGS;

/**
 * @enum SQFS_DIR_OPEN_FLAGS
 *
 * @brief Flags for @ref sqfs_dir_reader_open_dir
 */
typedef enum {
	/**
	 * @brief Do not generate "." and ".." entries
	 *
	 * If the @ref sqfs_dir_reader_t was created with
	 * the @ref SQFS_DIR_READER_DOT_ENTRIES flag set, "." and ".." entries
	 * are generated when iterating over a directory. If that is not desired
	 * in some instances, this flag can be set to suppress this behaviour
	 * when opening a directory.
	 */
	SQFS_DIR_OPEN_NO_DOT_ENTRIES = 0x00000001,

	SQFS_DIR_OPEN_ALL_FLAGS = 0x00000001,
} SQFS_DIR_OPEN_FLAGS;

/**
 * @brief sqfs_dir_reader_state_t
 *
 * @extends sqfs_readdir_state_t
 *
 * A meta data cursor for reading a directory, but with additional data
 * for the @ref sqfs_dir_reader_t, e.g. for resolving inodes.
 */
struct sqfs_dir_reader_state_t {
	/**
	 * @brief Base cursor structure for the directory meta data
	 */
	sqfs_readdir_state_t cursor;

	/**
	 * @brief An inode reference to the parent inode of the directory
	 *
	 * This is only available if the reader was created with
	 * the @ref SQFS_DIR_READER_DOT_ENTRIES flag and the behavior was
	 * not disabled when opening the directory.
	 */
	sqfs_u64 parent_ref;

	/**
	 * @brief A reference to the inode of the directory itself
	 *
	 * This is only available if the reader was created with
	 * the @ref SQFS_DIR_READER_DOT_ENTRIES flag and the behavior was
	 * not disabled when opening the directory.
	 */
	sqfs_u64 dir_ref;

	/**
	 * @brief A reference to the current entries inode
	 *
	 * After a successfull call to @ref sqfs_dir_reader_read, this contains
	 * a reference to the inode coresponding to the returned entry.
	 */
	sqfs_u64 ent_ref;

	/**
	 * @brief An opaque, internal state value
	 */
	sqfs_u8 state;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a directory reader.
 *
 * @memberof sqfs_dir_reader_t
 *
 * The function fails if any unknown flag is set. In squashfs-tools-ng
 * version 1.2 introduced the @ref SQFS_DIR_READER_DOT_ENTRIES flag,
 * earlier versions require the flags field to be set to zero.
 *
 * @param super A pointer to the super block.
 * @param cmp A compressor to use for unpacking meta data blocks.
 * @param file The input file to read from.
 * @param flags A combination of @ref SQFS_DIR_READER_FLAGS
 *
 * @return A new directory reader on success, NULL on allocation failure.
 */
SQFS_API sqfs_dir_reader_t *sqfs_dir_reader_create(const sqfs_super_t *super,
						   sqfs_compressor_t *cmp,
						   sqfs_file_t *file,
						   sqfs_u32 flags);

/**
 * @brief Navigate a directory reader to the location of a directory
 *        represented by an inode.
 *
 * @memberof sqfs_dir_reader_t
 *
 * This function seeks to the meta data block containing the directory
 * listing that the given inode referes to and resets the internal state.
 * After that, consequtive cals to @ref sqfs_dir_reader_read can be made
 * to iterate over the directory contents.
 *
 * If the reader was created with the @ref SQFS_DIR_READER_DOT_ENTRIES flag
 * set, the first two entries will be ".", referring to the directory inode
 * itself and "..", referring to the parent directory inode. Those entries
 * are generated artificially, as SquashFS does not store them on disk, hence
 * extra work is required and a flag is used to enable this behaviour. By
 * default, no such entries are generated.
 *
 * If this flag is set, but you wish to override that behaviour on a
 * per-instance basis, simply set the @ref SQFS_DIR_OPEN_NO_DOT_ENTRIES flag
 * when calling this function.
 *
 * @param rd A pointer to a directory reader.
 * @param inode An directory or extended directory inode.
 * @param flags A combination of @ref SQFS_DIR_OPEN_FLAGS.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_open_dir(sqfs_dir_reader_t *rd,
				      const sqfs_inode_generic_t *inode,
				      sqfs_dir_reader_state_t *state,
				      sqfs_u32 flags);

/**
 * @brief Read a directory entry and advance the internal position indicator
 *        to the next one.
 *
 * @memberof sqfs_dir_reader_t
 *
 * Call this function repeatedly to iterate over a directory listing. It
 * returns a positive number to indicate that it couldn't fetch any more data
 * because the end of the listing was reached. A negative value indicates an
 * error.
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a directory entry on success that can be
 *            freed with a single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure, a positive
 *         number if the end of the current directory listing has been reached.
 */
SQFS_API int sqfs_dir_reader_read(sqfs_dir_reader_t *rd,
				  sqfs_dir_reader_state_t *state,
				  sqfs_dir_node_t **out);

/**
 * @brief Resolve and deserialize an inode using an inode reference
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param ref An inode reference.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_inode(sqfs_dir_reader_t *rd, sqfs_u64 ref,
				       sqfs_inode_generic_t **inode);

/**
 * @brief Read the root inode using the location given by the super block.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_root_inode(sqfs_dir_reader_t *rd,
					    sqfs_inode_generic_t **inode);

/**
 * @brief Try to lookup a cached inode number
 *
 * @memberof sqfs_dir_reader_t
 *
 * If the inode reader was created with the @ref SQFS_DIR_READER_DOT_ENTRIES
 * flag, it maintains an internal cache of all directory inodes that it has
 * seen while traversing the filesystem. The cache maps inode numbers to
 * inode references. This is used to lookup the parent inode of directory.
 * This function can be used to query the cache directly.
 *
 * @param rd A pointer to a directory reader.
 * @param inode An inode number.
 * @param ref Retrns an inode reference on success.
 *
 * @return Zero on success, @ref SQFS_ERROR_NO_ENTRY if the inode is unknown
 */
SQFS_API int sqfs_dir_reader_resolve_inum(sqfs_dir_reader_t *rd,
					  sqfs_u32 inode, sqfs_u64 *ref);

/**
 * @brief Try to resole a path into an inode reference
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param path A path to resolve, NULL is interpreted as empty path.
 * @param root A directory inode to start from or NULL for the filesystem root.
 * @param out Retrns an inode reference on success.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure.
 */
SQFS_API int sqfs_dir_reader_resolve_path(sqfs_dir_reader_t *rd,
					  const char *path,
					  const sqfs_inode_generic_t *root,
					  sqfs_u64 *out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_READER_H */
