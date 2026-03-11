/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * meta_writer.h - This file is part of libsquashfs
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
#ifndef SQFS_META_WRITER_H
#define SQFS_META_WRITER_H

#include "sqfs/predef.h"

/**
 * @file meta_writer.h
 *
 * @brief Contains declarations for the @ref sqfs_meta_writer_t.
 */

/**
 * @struct sqfs_meta_writer_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts generating of meta data blocks, either in memory or
 *        directly on disk.
 *
 * SquashFS stores meta data by dividing it into fixed size (8k) chunks
 * that are written to disk with a small header indicating the on-disk
 * size and whether it is compressed or not.
 *
 * Data written to meta data blocks doesn't have to be aligned, i.e.
 * SquashFS doesn't care if an object is written across two blocks.
 *
 * The main task of the meta data writer is to provide a simple append
 * function that transparently takes care of chopping data up into blocks,
 * compressing the blocks and pre-pending a header.
 *
 * This object is not copyable, i.e. @ref sqfs_copy will always return NULL.
 */

/**
 * @enum SQFS_META_WRITER_FLAGS
 *
 * @brief Possible flags for @ref sqfs_meta_writer_create.
 */
typedef enum {
	/**
	 * @brief If set, keep finished blocks in memory.
	 *
	 * To write them to disk, explicitly call
	 * @ref sqfs_meta_write_write_to_file.
	 */
	SQFS_META_WRITER_KEEP_IN_MEMORY = 0x01,

	SQFS_META_WRITER_ALL_FLAGS = 0x01,
} SQFS_META_WRITER_FLAGS;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a meta data writer.
 *
 * @memberof sqfs_meta_writer_t
 *
 * @note The meta writer internally keeps references to the pointers passed
 *       to this function, so don't destroy them before destroying the
 *       meta writer.
 *
 * @param file An output file to write the data to.
 * @param cmp A compressor to use.
 * @param flags A combination of @ref SQFS_META_WRITER_FLAGS.
 *
 * @return A pointer to a meta writer on success, NULL on allocation failure
 *         or if an unknown flag was set.
 */
SQFS_API sqfs_meta_writer_t *sqfs_meta_writer_create(sqfs_file_t *file,
						     sqfs_compressor_t *cmp,
						     sqfs_u32 flags);

/**
 * @brief Finish the current block, even if it isn't full yet.
 *
 * @memberof sqfs_meta_writer_t
 *
 * This function forces the meta writer to compress and store the block it
 * is currently writing to, even if it isn't full yet, and either write it
 * out to disk (or append it to the in memory chain if told to keep blocks
 * in memory).
 *
 * @param m A pointer to a meta data writer.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_writer_flush(sqfs_meta_writer_t *m);

/**
 * @brief Finish the current block, even if it isn't full yet.
 *
 * @memberof sqfs_meta_writer_t
 *
 * This function forces reads a speicifed number of bytes from a given data
 * block and appends it to the meta data block that the writer is currently
 * working on. If the block becomes full, it is compressed, written to disk
 * and a new block is started that the remaining data is written to.
 *
 * @param m A pointer to a meta data writer.
 * @param data A pointer to a chunk of data to append to the writer.
 * @param size The number of data bytes to append.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_writer_append(sqfs_meta_writer_t *m, const void *data,
				     size_t size);

/**
 * @brief Query the current block start position and offset within the block
 *
 * @memberof sqfs_meta_writer_t
 *
 * Get the byte offset relative to the first block that the current block will
 * start at once it is written to disk and get the byte offset within this
 * block that the next call to @ref sqfs_meta_writer_append will start writing
 * data at.
 *
 * @param m A pointer to a meta data writer.
 * @param block_start Returns the offset of the current block from the first.
 * @param offset Returns an offset into the current block where the next write
 *               starts.
 */
SQFS_API void sqfs_meta_writer_get_position(const sqfs_meta_writer_t *m,
					    sqfs_u64 *block_start,
					    sqfs_u32 *offset);

/**
 * @brief Reset all internal state, including the current block start position.
 *
 * @memberof sqfs_meta_writer_t
 *
 * This functions forces the meta data writer to forget everything that
 * happened since it was created, so it can be recycled.
 *
 * The data written is not lost, unless it was kept in memory and never written
 * out to disk.
 */
SQFS_API void sqfs_meta_writer_reset(sqfs_meta_writer_t *m);

/**
 * @brief Write all blocks collected in memory to disk
 *
 * @memberof sqfs_meta_writer_t
 *
 * If the meta writer was created with the flag set to store blocks in
 * memory instead of writing them to disk, calling this function forces
 * the meta writer to write out all blocks it collected so far.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_write_write_to_file(sqfs_meta_writer_t *m);

/**
 * @brief A convenience function for encoding and writing an inode
 *
 * @memberof sqfs_meta_writer_t
 *
 * The SquashFS inode table is essentially a series of meta data blocks
 * containing variable sized inodes. This function takes a generic inode
 * structure, encodes it and writes the result to a meta data writer
 * using @ref sqfs_meta_writer_append internally.
 *
 * @param iw A pointer to a meta data writer.
 * @param n A pointer to an inode.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_meta_writer_write_inode(sqfs_meta_writer_t *iw,
					  const sqfs_inode_generic_t *n);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_META_WRITER_H */
