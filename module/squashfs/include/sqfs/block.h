/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block.h - This file is part of libsquashfs
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
#ifndef SQFS_BLOCK_H
#define SQFS_BLOCK_H

#include "sqfs/predef.h"

/**
 * @file block.h
 *
 * @brief Contains on-disk data structures for data block management.
 */

#define SQFS_META_BLOCK_SIZE 8192

#define SQFS_IS_BLOCK_COMPRESSED(size) (((size) & (1 << 24)) == 0)
#define SQFS_ON_DISK_BLOCK_SIZE(size) ((size) & ((1 << 24) - 1))
#define SQFS_IS_SPARSE_BLOCK(size) (SQFS_ON_DISK_BLOCK_SIZE(size) == 0)

/**
 * @struct sqfs_fragment_t
 *
 * @brief Data structure that makes up the fragment table entries.
 */
struct sqfs_fragment_t {
	/**
	 * @brief Location of the fragment block on-disk.
	 */
	sqfs_u64 start_offset;

	/**
	 * @brief Size of the fragment block in bytes.
	 */
	sqfs_u32 size;

	/**
	 * @brief Unused. Always initialize this to 0.
	 */
	sqfs_u32 pad0;
};

/**
 * @enum SQFS_BLK_FLAGS
 *
 * @brief Generic flags that tell the processor what to do with a block and
 *        flags that the processor sets when it is done with a block.
 */
typedef enum {
	/**
	 * @brief Only calculate checksum, do NOT compress the data.
	 *
	 * If set, the blocks of a file will not be compressed by the
	 * @ref sqfs_block_processor_t.
	 */
	SQFS_BLK_DONT_COMPRESS = 0x0001,

	/**
	 * @brief Don't compute block data checksums.
	 */
	SQFS_BLK_DONT_HASH = 0x0002,

	/**
	 * @brief Don't add the tail end of a file to a fragment block.
	 *
	 * If set, the @ref sqfs_block_processor_t will always generate a final
	 * block for a file, even if it is truncated. It will not add the
	 * tail end to a fragment block.
	 */
	SQFS_BLK_DONT_FRAGMENT = 0x0004,

	/**
	 * @brief Surpress deduplication.
	 *
	 * If set on fragments or the last block of a file, it is always
	 * written to disk, even if a duplicate copy already exists.
	 */
	SQFS_BLK_DONT_DEDUPLICATE = 0x0008,

	/**
	 * @brief Supress sparse block detection.
	 *
	 * If set, sparse blocks are no longer checked and flagged as such and
	 * are instead processed like normal blocks.
	 */
	SQFS_BLK_IGNORE_SPARSE = 0x0010,

	/**
	 * @brief Set by the @ref sqfs_block_processor_t if it determines a
	 *        block of a file to be sparse, i.e. only zero bytes.
	 */
	SQFS_BLK_IS_SPARSE = 0x0400,

	/**
	 * @brief Set by the @ref sqfs_block_processor_t on the first
	 *        block of a file.
	 */
	SQFS_BLK_FIRST_BLOCK = 0x0800,

	/**
	 * @brief Set by the @ref sqfs_block_processor_t on the last
	 *        block of a file.
	 */
	SQFS_BLK_LAST_BLOCK = 0x1000,

	/**
	 * @brief Set by the @ref sqfs_block_processor_t to indicate that a
	 *        block is a tail end of a file and the block.
	 */
	SQFS_BLK_IS_FRAGMENT = 0x2000,

	/**
	 * @brief Set by the @ref sqfs_block_processor_t on fragment blocks that
	 *        it generates.
	 */
	SQFS_BLK_FRAGMENT_BLOCK = 0x4000,

	/**
	 * @brief Set by @ref sqfs_block_processor_t if the block was
	 *        actually compressed.
	 */
	SQFS_BLK_IS_COMPRESSED = 0x8000,

	/**
	 * @brief The combination of all flags that are user settable.
	 */
	SQFS_BLK_USER_SETTABLE_FLAGS = 0x001F,

	SQFS_BLK_FLAGS_ALL = 0xFC1F,
} SQFS_BLK_FLAGS;

#endif /* SQFS_BLOCK_H */
