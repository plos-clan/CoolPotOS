/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_writer.h - This file is part of libsquashfs
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
#ifndef SQFS_BLOCK_WRITER_H
#define SQFS_BLOCK_WRITER_H

#include "sqfs/predef.h"

/**
 * @file block_writer.h
 *
 * @brief Contains declarations for the @ref sqfs_block_writer_t structure.
 */

/**
 * @interface sqfs_block_writer_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts writing and deduplicating of data and fragment blocks.
 *
 * A default reference implementation can be obtaiend
 * through @ref sqfs_block_writer_create. The default implementation is not
 * copyable, i.e. @ref sqfs_copy will always return NULL.
 */
struct sqfs_block_writer_t {
	sqfs_object_t base;

	/**
	 * @brief Submit a data block to a block writer.
	 *
	 * @memberof sqfs_block_writer_t
	 *
	 * If the @ref SQFS_BLK_FIRST_BLOCK flag is set, the data block writer
	 * memorizes the starting location and block index of the block. If the
	 * @ref SQFS_BLK_LAST_BLOCK flag is set, it uses those stored locations
	 * to do block deduplication.
	 *
	 * @param wr A pointer to a block writer.
	 * @param user An optional user data pointer.
	 *             The @ref sqfs_block_processor_t can be told to pass this
	 *             on to the block writer for each block.
	 * @param size The size of the block to write.
	 * @param checksum A 32 bit checksum of the block data.
	 * @param flags A combination of @ref SQFS_BLK_FLAGS flag bits
	 *              describing the block.
	 * @arapm data A pointer to the data to write.
	 * @param location Returns the location where the block has been
	 *        written. If the @ref SQFS_BLK_LAST_BLOCK flag was set,
	 *        deduplication is performed and this returns the (new) location
	 *        of the first block instead.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR error on failure.
	 */
	int (*write_data_block)(sqfs_block_writer_t *wr, void *user,
				sqfs_u32 size, sqfs_u32 checksum,
				sqfs_u32 flags, const sqfs_u8 *data,
				sqfs_u64 *location);

	/**
	 * @brief Get the number of blocks actually written to disk.
	 *
	 * @memberof sqfs_block_writer_t
	 *
	 * @param wr A pointer to a block writer.
	 *
	 * @return The number of blocks written, excluding deduplicated blocks.
	 */
	sqfs_u64 (*get_block_count)(const sqfs_block_writer_t *wr);
};

/**
 * @enum SQFS_BLOCK_WRITER_FLAGS
 *
 * @brief Flags that can be passed to @ref sqfs_block_writer_create
 */
typedef enum {
	/**
	 * @brief If set, only compare checksums when deduplicating blocks.
	 *
	 * Since squashfs-tools-ng version 1.1, the default for the block
	 * writer is to compare checksum & size for blocks during deduplication
	 * and then read the potential match back from disk and do a byte for
	 * byte comparison to make absolutely sure they match.
	 *
	 * If this flag is set, the hash & size check is treated as being
	 * sufficient for block deduplication, which does increase performance,
	 * but risks data loss or corruption if a hash collision occours.
	 */
	SQFS_BLOCK_WRITER_HASH_COMPARE_ONLY = 0x01,

	/**
	 * @brief A combination of all valid flags.
	 */
	SQFS_BLOCK_WRITER_ALL_FLAGS = 0x01
} SQFS_BLOCK_WRITER_FLAGS;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an instance of a default block writer implementation.
 *
 * @memberof sqfs_block_writer_t
 *
 * @param file A pointer to a file object that data should be appended to.
 * @param flags A combination of @ref SQFS_BLOCK_WRITER_FLAGS values. If an
 *              unknown flag is set, creation will fail.
 *
 * @return A pointer to a new block writer on success, NULL on failure.
 */
SQFS_API sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file,
						       sqfs_u32 flags);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_BLOCK_WRITER_H */
