/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * frag_table.h - This file is part of libsquashfs
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
#ifndef SQFS_FRAG_TABLE_H
#define SQFS_FRAG_TABLE_H

#include "sqfs/predef.h"

/**
 * @file frag_table.h
 *
 * @brief Contains declarations for the @ref sqfs_frag_table_t data structure.
 */

/**
 * @struct sqfs_frag_table_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts reading, writing and management of the fragment table.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a fragment table.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param flags Currently must be set to 0, otherwise this function fails.
 *
 * @return A pointer to a new fragment table object on success, NULL on failure.
 */
SQFS_API sqfs_frag_table_t *sqfs_frag_table_create(sqfs_u32 flags);

/**
 * @brief Load a fragment table from a SquashFS image.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param tbl The fragment table object to load into.
 * @param file The file to load the table from.
 * @param super The super block that describes the location
 *              and size of the table.
 * @param cmp The compressor to use for uncompressing the table.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_frag_table_read(sqfs_frag_table_t *tbl, sqfs_file_t *file,
				  const sqfs_super_t *super,
				  sqfs_compressor_t *cmp);

/**
 * @brief Write a fragment table to a SquashFS image.
 *
 * @memberof sqfs_frag_table_t
 *
 * The data from the table is compressed and appended to the given file. The
 * information about the tables size and location are stored in the given super
 * block. Super block flags are updated as well.
 *
 * @param tbl The fragment table object to store on disk.
 * @param file The file to append the table to.
 * @param super The super block that should be updated with the location
 *              and size of the table.
 * @param cmp The compressor to use for compressing the table.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure.
 */
SQFS_API int sqfs_frag_table_write(sqfs_frag_table_t *tbl, sqfs_file_t *file,
				   sqfs_super_t *super, sqfs_compressor_t *cmp);

/**
 * @brief Resolve a fragment block index to its description.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param tbl A pointer to the fragmen table object.
 * @param index The index into the table.
 * @param out Returns the data from the table on success.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure (e.g. index is
 *         out of bounds).
 */
SQFS_API int sqfs_frag_table_lookup(sqfs_frag_table_t *tbl, sqfs_u32 index,
				    sqfs_fragment_t *out);

/**
 * @brief Append a table entry to a fragment table.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param tbl A pointer to the fragmen table object.
 * @param location The absolute on-disk location of the new fragment block.
 * @param out The size of the block. Has bit 24 set if compressed.
 * @param index If not NULL, returns the allocated table index.
 *
 * @return Zero on success, an @ref SQFS_ERROR on failure (e.g. allocation
 *         failure).
 */
SQFS_API int sqfs_frag_table_append(sqfs_frag_table_t *tbl, sqfs_u64 location,
				    sqfs_u32 size, sqfs_u32 *index);

/**
 * @brief Modify an existing entry in a fragment table.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param tbl A pointer to the fragmen table object.
 * @param index The fragment table index.
 * @param location The absolute on-disk location of the fragment block.
 * @param out The size of the block. Has bit 24 set if compressed.
 *
 * @return Zero on success, an @ref SQFS_ERROR on
 *         failure (e.g. out of bounds).
 */
SQFS_API int sqfs_frag_table_set(sqfs_frag_table_t *tbl, sqfs_u32 index,
				 sqfs_u64 location, sqfs_u32 size);

/**
 * @brief Get the number of entries stored in a fragment table.
 *
 * @memberof sqfs_frag_table_t
 *
 * @param tbl A pointer to the fragmen table object.
 *
 * @return The number of entries currently stored in the table.
 */
SQFS_API size_t sqfs_frag_table_get_size(sqfs_frag_table_t *tbl);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_FRAG_TABLE_H */
