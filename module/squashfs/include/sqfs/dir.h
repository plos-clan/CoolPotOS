/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir.h - This file is part of libsquashfs
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
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "sqfs/predef.h"

/**
 * @file dir.h
 *
 * @brief Contains on-disk data structures for the directory table and
 *        declarations for the @ref sqfs_dir_writer_t.
 */

#define SQFS_MAX_DIR_ENT 256

/**
 * @struct sqfs_dir_header_t
 *
 * @brief On-disk data structure of a directory header
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_header_t {
	/**
	 * @brief The number of @ref sqfs_dir_node_t entries that are
	 *        following.
	 *
	 * This value is stored off by one and the total count must not
	 * exceed 256.
	 */
	sqfs_u32 count;

	/**
	 * @brief The location of the meta data block containing the inodes for
	 *        the entries that follow, relative to the start of the inode
	 *        table.
	 */
	sqfs_u32 start_block;

	/**
	 * @brief The inode number of the first entry.
	 */
	sqfs_u32 inode_number;
};

/**
 * @struct sqfs_dir_node_t
 *
 * @brief On-disk data structure of a directory entry. Many of these
 *        follow a single @ref sqfs_dir_header_t.
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_node_t {
	/**
	 * @brief An offset into the uncompressed meta data block containing
	 *        the coresponding inode.
	 */
	sqfs_u16 offset;

	/**
	 * @brief Signed difference of the inode number from the one
	 *        in the @ref sqfs_dir_header_t.
	 */
	sqfs_s16 inode_diff;

	/**
	 * @brief The @ref SQFS_INODE_TYPE value for the inode that this
	 *        entry represents.
	 */
	sqfs_u16 type;

	/**
	 * @brief The size of the entry name
	 *
	 * This value is stored off-by-one.
	 */
	sqfs_u16 size;

	/**
	 * @brief The name of the directory entry (no trailing null-byte).
	 */
	sqfs_u8 name[];
};

/**
 * @struct sqfs_dir_index_t
 *
 * @brief On-disk data structure of a directory index. A series of those
 *        can follow an @ref sqfs_inode_dir_ext_t.
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_index_t {
	/**
	 * @brief Linear byte offset into the decompressed directory listing.
	 */
	sqfs_u32 index;

	/**
	 * @brief Location of the meta data block, relative to the directory
	 *        table start.
	 */
	sqfs_u32 start_block;

	/**
	 * @brief Size of the name of the first entry after the header.
	 *
	 * This value is stored off-by-one.
	 */
	sqfs_u32 size;

	/**
	 * @brief Name of the name of the first entry after the header.
	 *
	 * No trailing null-byte.
	 */
	sqfs_u8 name[];
};

#endif /* SQFS_DIR_H */
