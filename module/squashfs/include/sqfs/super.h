/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * super.h - This file is part of libsquashfs
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
#ifndef SQFS_SUPER_H
#define SQFS_SUPER_H

#include "sqfs/predef.h"

/**
 * @file super.h
 *
 * @brief Contains on-disk data structures, identifiers and functions for the
 *        SquashFS super block.
 */

#define SQFS_MAGIC 0x73717368
#define SQFS_VERSION_MAJOR 4
#define SQFS_VERSION_MINOR 0
#define SQFS_DEVBLK_SIZE 4096

#define SQFS_MIN_BLOCK_SIZE (4 * 1024)
#define SQFS_MAX_BLOCK_SIZE (1024 * 1024)
#define SQFS_DEFAULT_BLOCK_SIZE (128 * 1024)

/**
 * @struct sqfs_super_t
 *
 * @brief The SquashFS super block, located at the beginning of the file system
 *        to describe the layout of the filesystem.
 */
struct sqfs_super_t {
	/**
	 * @brief Magic number. Must be set to SQFS_MAGIC.
	 */
	sqfs_u32 magic;

	/**
	 * @brief Total number of inodes.
	 */
	sqfs_u32 inode_count;

	/**
	 * @brief Last time the filesystem was modified.
	 *
	 * This field counts seconds (not counting leap seconds) since 00:00,
	 * Jan 1 1970 UTC. This field is unsigned, so it expires in the year
	 * 2106 (as opposed to 2038).
	 */
	sqfs_u32 modification_time;

	/**
	 * @brief The data block size in bytes.
	 *
	 * Must be a power of 2, no less than 4k and not larger than 1M.
	 */
	sqfs_u32 block_size;

	/**
	 * @brief The number of fragment blocks in the data area.
	 */
	sqfs_u32 fragment_entry_count;

	/**
	 * @brief Identifies the compressor that has been used.
	 *
	 * Valid identifiers are in the @ref SQFS_COMPRESSOR enum.
	 */
	sqfs_u16 compression_id;

	/**
	 * @brief The log2 of the block_size field for sanity checking
	 *
	 * Must be no less than 12 and not larger than 20.
	 */
	sqfs_u16 block_log;

	/**
	 * @brief A combination of @ref SQFS_SUPER_FLAGS flags
	 *
	 * Most of the flags that can be set here are informative only.
	 */
	sqfs_u16 flags;

	/**
	 * @brief The total number of unique user or group IDs.
	 */
	sqfs_u16 id_count;

	/**
	 * @brief Must be @ref SQFS_VERSION_MAJOR
	 */
	sqfs_u16 version_major;

	/**
	 * @brief Must be @ref SQFS_VERSION_MINOR
	 */
	sqfs_u16 version_minor;

	/**
	 * @brief A reference to the root inode
	 *
	 * The bits 16 to 48 hold an offset that is added to inode_table_start
	 * to get the location of the meta data block containing the inode.
	 * The lower 16 bits hold a byte offset into the uncompressed block.
	 */
	sqfs_u64 root_inode_ref;

	/**
	 * @brief Total size of the file system in bytes, not counting padding
	 */
	sqfs_u64 bytes_used;

	/**
	 * @brief On-disk location of the ID table
	 *
	 * This value must point to a location after the directory table and
	 * (if present) after the export and fragment tables, but before the
	 * xattr table.
	 */
	sqfs_u64 id_table_start;

	/**
	 * @brief On-disk location of the extended attribute table (if present)
	 *
	 * See @ref sqfs_xattr_reader_t for an overview on how SquashFS stores
	 * extended attributes on disk.
	 *
	 * This value must either point to a location after the ID table, or
	 * it must be set to 0xFFFFFFFF to indicate the table is not present.
	 */
	sqfs_u64 xattr_id_table_start;

	/**
	 * @brief On-disk location of the first meta data block containing
	 *        the inodes
	 *
	 * This value must point to a location before the directory table.
	 */
	sqfs_u64 inode_table_start;

	/**
	 * @brief On-disk location of the first meta data block containing
	 *        the directory entries
	 *
	 * This value must point to a location after the inode table but
	 * before the fragment, export, ID and xattr tables.
	 */
	sqfs_u64 directory_table_start;

	/**
	 * @brief On-disk location of the fragment table (if present)
	 *
	 * This value must either point to a location after the directory
	 * table, but before the export, ID and xattr tables, or it must be
	 * set to 0xFFFFFFFF to indicate that the table is not present.
	 */
	sqfs_u64 fragment_table_start;

	/**
	 * @brief On-disk location of the export table (if present)
	 *
	 * This value must either point to a location after directory table
	 * (and if present after the fragment table), but before the ID table,
	 * or it must be set to 0xFFFFFFFF to indicate that the table is not
	 * present.
	 */
	sqfs_u64 export_table_start;
};

/**
 * @enum SQFS_COMPRESSOR
 *
 * @brief Set in @ref sqfs_super_t to identify the compresser used by the
 *        filesystem.
 *
 * Most of the flags that can be set are informative only.
 */
typedef enum {
	SQFS_COMP_GZIP = 1,
	SQFS_COMP_LZMA = 2,
	SQFS_COMP_LZO = 3,
	SQFS_COMP_XZ = 4,
	SQFS_COMP_LZ4 = 5,
	SQFS_COMP_ZSTD = 6,

	SQFS_COMP_MIN = 1,
	SQFS_COMP_MAX = 6,
} SQFS_COMPRESSOR;

/**
 * @enum SQFS_SUPER_FLAGS
 *
 * @brief Flags that can be set in @ref sqfs_super flags field.
 */
typedef enum {
	/**
	 * @brief Set to indicate that meta data blocks holding the inodes are
	 *        stored uncompressed.
	 */
	SQFS_FLAG_UNCOMPRESSED_INODES = 0x0001,

	/**
	 * @brief Set to indicate that all data blocks are stored uncompressed.
	 */
	SQFS_FLAG_UNCOMPRESSED_DATA = 0x0002,

	/**
	 * @brief Set to indicate that all fragment blocks are stored
	 *        uncompressed.
	 */
	SQFS_FLAG_UNCOMPRESSED_FRAGMENTS = 0x0008,

	/**
	 * @brief Set to indicate that there are no fragment blocks.
	 */
	SQFS_FLAG_NO_FRAGMENTS = 0x0010,

	/**
	 * @brief Set to indicate that fragments have been generated for all
	 *        files that are not a multiple of the block size in size.
	 */
	SQFS_FLAG_ALWAYS_FRAGMENTS = 0x0020,

	/**
	 * @brief Set to indicate that data blocks have not been deduplicated.
	 */
	SQFS_FLAG_NO_DUPLICATES = 0x0040,

	/**
	 * @brief Set to indicate that an NFS export table is present.
	 */
	SQFS_FLAG_EXPORTABLE = 0x0080,

	/**
	 * @brief Set to indicate that meta data blocks holding extended
	 *        attributes are stored uncompressed.
	 */
	SQFS_FLAG_UNCOMPRESSED_XATTRS = 0x0100,

	/**
	 * @brief Set to indicate that the filesystem does not
	 *        contain extended attributes.
	 */
	SQFS_FLAG_NO_XATTRS = 0x0200,

	/**
	 * @brief Set to indicate that a single, uncompressed meta data block
	 *        with compressor options follows the super block.
	 */
	SQFS_FLAG_COMPRESSOR_OPTIONS = 0x0400,

	/**
	 * @brief Set to indicate that meta data blocks holding the IDs are
	 *        stored uncompressed.
	 */
	SQFS_FLAG_UNCOMPRESSED_IDS = 0x0800,
} SQFS_SUPER_FLAGS;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the SquashFS super block.
 *
 * @memberof sqfs_super_t
 *
 * @param super A pointer to a super block structure.
 * @param block_size The uncompressed size of the data blocks in bytes.
 * @param mtime The modification time stamp to set.
 * @param compressor The compressor ID to set.
 *
 * @return Zero on success, an @ref SQFS_ERROR value if one of the
 *         fields does not hold a valid value.
 */
SQFS_API int sqfs_super_init(sqfs_super_t *super, size_t block_size,
			     sqfs_u32 mtime,
			     SQFS_COMPRESSOR compressor);

/**
 * @brief Encode and write a SquashFS super block to disk.
 *
 * @memberof sqfs_super_t
 *
 * @param super A pointer to the super block structure to write.
 * @param file A file object through which to access the filesystem image.
 *
 * @return Zero on success, an @ref SQFS_ERROR value if one of the
 *         fields does not hold a valid value.
 */
SQFS_API int sqfs_super_write(const sqfs_super_t *super, sqfs_file_t *file);

/**
 * @brief Read a SquashFS super block from disk, decode it and check the fields
 *
 * @memberof sqfs_super_t
 *
 * @param super A pointer to the super block structure to fill.
 * @param file A file object through which to access the filesystem image.
 *
 * @return Zero on success, an @ref SQFS_ERROR value if one of the
 *         fields does not hold a valid value.
 */
SQFS_API int sqfs_super_read(sqfs_super_t *super, sqfs_file_t *file);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_SUPER_H */
