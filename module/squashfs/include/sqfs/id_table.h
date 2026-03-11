/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * id_table.h - This file is part of libsquashfs
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
#ifndef SQFS_ID_TABLE_H
#define SQFS_ID_TABLE_H

#include "sqfs/predef.h"

/**
 * @file id_table.h
 *
 * @brief Contains declarations for the @ref sqfs_id_table_t data structure.
 */

/**
 * @struct sqfs_id_table_t
 *
 * @implements sqfs_object_t
 *
 * @brief A simple data structure that encapsulates ID to index mapping for
 *        user and group IDs.
 *
 * SquashFS does not store user and group IDs in inodes directly. Instead, it
 * collects the unique 32 bit IDs in a table with at most 64k entries and
 * stores a 16 bit index into the inode. This allows SquashFS to only have 16
 * bit UID/GID entries in the inodes but actually have 32 bit UIDs/GIDs under
 * the hood (at least 64k selected ones).
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an ID table object.
 *
 * @memberof sqfs_id_table_t
 *
 * @param flags Currently must be set to 0 or creating the table fails.
 *
 * @return A pointer to an ID table object, NULL on allocation failure.
 */
SQFS_API sqfs_id_table_t *sqfs_id_table_create(sqfs_u32 flags);

/**
 * @brief Resolve a 32 bit ID to a unique 16 bit index.
 *
 * @memberof sqfs_id_table_t
 *
 * @param tbl A pointer to an ID table object.
 * @param id The ID to resolve.
 * @param out Returns the unique table index.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_id_table_id_to_index(sqfs_id_table_t *tbl, sqfs_u32 id,
				       sqfs_u16 *out);

/**
 * @brief Write an ID table to disk.
 *
 * @memberof sqfs_id_table_t
 *
 * @param tbl A pointer to an ID table object.
 * @param file The underlying file to append the table to.
 * @param super A pointer to a super block in which to store the ID table
 *              start location.
 * @param cmp A compressor to use to compress the ID table.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_id_table_write(sqfs_id_table_t *tbl, sqfs_file_t *file,
				 sqfs_super_t *super, sqfs_compressor_t *cmp);

/**
 * @brief Read an ID table from disk.
 *
 * @memberof sqfs_id_table_t
 *
 * @param tbl A pointer to an ID table object.
 * @param file The underlying file to read the table from.
 * @param super A pointer to a super block from which to get
 *              the ID table location.
 * @param cmp A compressor to use to extract compressed table blocks.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_id_table_read(sqfs_id_table_t *tbl, sqfs_file_t *file,
				const sqfs_super_t *super,
				sqfs_compressor_t *cmp);

/**
 * @brief Resolve a 16 bit index to a 32 bit ID.
 *
 * @memberof sqfs_id_table_t
 *
 * @param tbl A pointer to an ID table object.
 * @param index The table index to resolve.
 * @param out Returns the underlying 32 bit ID that the index maps to.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_id_table_index_to_id(const sqfs_id_table_t *tbl,
				       sqfs_u16 index, sqfs_u32 *out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_ID_TABLE_H */
