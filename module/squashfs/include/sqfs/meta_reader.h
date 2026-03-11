/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * meta_reader.h - This file is part of libsquashfs
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
#ifndef SQFS_META_READER_H
#define SQFS_META_READER_H

#include "sqfs/predef.h"
#include "sqfs/super.h"

/**
 * @file meta_reader.h
 *
 * @brief Contains declarations for the @ref sqfs_meta_reader_t.
 */

/**
 * @struct sqfs_meta_reader_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts reading of meta data blocks.
 *
 * SquashFS stores meta data by dividing it into fixed size (8k) chunks
 * that are written to disk with a small header indicating the on-disk
 * size and whether it is compressed or not.
 *
 * Data written to meta data blocks doesn't have to be aligned, i.e.
 * SquashFS doesn't care if an object is written across two blocks.
 *
 * The main task of the meta data read is to provide a simple read and seek
 * functions that transparently take care of fetching and uncompressing blocks
 * from disk and reading transparently across block boarders if required.
 */

/**
 * @struct sqfs_readdir_state_t
 *
 * @brief Encapsulates state for simple directory reading
 */
struct sqfs_readdir_state_t {
	sqfs_u64 inode_block;
	sqfs_u64 block;
	size_t offset;
	size_t size;
	size_t entries;
	sqfs_u32 inum_base;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a meta data reader
 *
 * @memberof sqfs_meta_reader_t
 *
 * @param file A pointer to a file object to read from
 * @param cmp A compressor to use for unpacking compressed meta data blocks
 * @param start A lower limit for the blocks to be read. Every seek to an offset
 *              below that is interpreted as an out-of-bounds read.
 * @param limit An upper limit for the blocks to read. Every seek to an offset
 *              afer that is interpreted as an out-of-bounds read.
 *
 * @return A pointer to a meta data reader on success, NULL on
 *         allocation failure.
 */
SQFS_API sqfs_meta_reader_t *sqfs_meta_reader_create(sqfs_file_t *file,
						     sqfs_compressor_t *cmp,
						     sqfs_u64 start,
						     sqfs_u64 limit);

/**
 * @brief Seek to a specific meta data block and offset.
 *
 * @memberof sqfs_meta_reader_t
 *
 * The underlying block is fetched from disk and uncompressed, unless it
 * already is the current block.
 *
 * @param m A pointer to a meta data reader.
 * @param block_start Absolute position where the block header can be found.
 * @param offset A byte offset into the uncompressed block.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_reader_seek(sqfs_meta_reader_t *m, sqfs_u64 block_start,
				   size_t offset);

/**
 * @brief Get the current position that the next read will read from.
 *
 * @memberof sqfs_meta_reader_t
 *
 * @param m A pointer to a meta data reader.
 * @param block_start Absolute position where the current block is.
 * @param offset A byte offset into the uncompressed block.
 */
SQFS_API void sqfs_meta_reader_get_position(const sqfs_meta_reader_t *m,
					    sqfs_u64 *block_start,
					    size_t *offset);

/**
 * @brief Read a chunk of data from a meta data reader.
 *
 * @memberof sqfs_meta_reader_t
 *
 * If the meta data reader reaches the end of the current block before filling
 * the destination buffer, it transparently reads the next block from disk and
 * uncompresses it.
 *
 * @param m A pointer to a meta data reader.
 * @param data A pointer to copy the data to.
 * @param size The numbre of bytes to read.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_reader_read(sqfs_meta_reader_t *m, void *data,
				   size_t size);

/**
 * @brief Read and decode a directory header from a meta data reader.
 *
 * @memberof sqfs_meta_reader_t
 *
 * This is a convenience function on to of @ref sqfs_meta_reader_read that
 * reads and decodes a directory header from a meta data block.
 *
 * @param m A pointer to a meta data reader.
 * @param hdr A pointer to a directory header to fill.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
					      sqfs_dir_header_t *hdr);

/**
 * @brief Read and decode a directory header from a meta data reader.
 *
 * @memberof sqfs_meta_reader_t
 *
 * This is a convenience function on to of @ref sqfs_meta_reader_read that
 * reads and decodes a directory entry.
 *
 * @param m A pointer to a meta data reader.
 * @param ent Returns a pointer to a directory entry. Can be released with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m,
					   sqfs_dir_node_t **ent);

/**
 * @brief Initialize a state object for reading a directory
 *
 * @memberof sqfs_readdir_state_t
 *
 * This function initializes a simple state object to point to the
 * location of a directory header and store the total, uncompressed
 * size of the directory.
 *
 * The state object can be passed to @ref sqfs_meta_reader_readdir
 * to read entries one-by-one.
 *
 * @param s A pointer to the state object to initialize.
 * @param super A pointer to the super block, telling us where
 *              the directory table starts.
 * @param inode A pointer to a directory inode from which to get the
 *              directory location.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on
 *         failure (e.g. the inode is not a directory inode).
 */
SQFS_API
int sqfs_readdir_state_init(sqfs_readdir_state_t *s, const sqfs_super_t *super,
			    const sqfs_inode_generic_t *inode);

/**
 * @brief Simple directory reading interface
 *
 * @memberof sqfs_meta_reader_t
 *
 * This function successively reads directory entries, transparently
 * parsing and skipping across headers. The state is encapsulated in
 * an external object that is passed in and the function seeks to the
 * location, so one can swap between multiple states and read several
 * directories interchangeably.
 *
 * @param m A pointer to a meta data reader.
 * @param s A pointer to a directory state that is used and updated.
 * @param ent Returns a pointer to a directory entry. Can be
 *            released with a single @ref sqfs_free call.
 * @param inum If not NULL, returns the decoded inode number.
 * @param iref If not NULL, returns a reference to the inode.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure,
 *         a positive number if the end of the directory is reached.
 */
SQFS_API int sqfs_meta_reader_readdir(sqfs_meta_reader_t *m,
				      sqfs_readdir_state_t *s,
				      sqfs_dir_node_t **ent,
				      sqfs_u32 *inum, sqfs_u64 *iref);

/**
 * @brief Read and decode an inode from a meta data reader.
 *
 * @memberof sqfs_meta_reader_t
 *
 * This is a convenience function on to of @ref sqfs_meta_reader_seek and
 * @ref sqfs_meta_reader_read that reads and decodes an inode.
 *
 * @param ir A pointer to a meta data reader.
 * @param super A pointer to the super block, required for figuring out the
 *              size of file inodes.
 * @param block_start The meta data block to seek to for reading the inode.
 * @param offset A byte offset within the uncompressed block where the
 *               inode is.
 * @param out Returns a pointer to an inode. Can be released with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_meta_reader_read_inode(sqfs_meta_reader_t *ir,
				const sqfs_super_t *super,
				sqfs_u64 block_start, size_t offset,
				sqfs_inode_generic_t **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_META_READER_H */
