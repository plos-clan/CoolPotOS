/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer.h - This file is part of libsquashfs
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
#ifndef SQFS_XATRR_WRITER_H
#define SQFS_XATRR_WRITER_H

#include "sqfs/predef.h"

/**
 * @file xattr_writer.h
 *
 * @brief Contains declarations for the @ref sqfs_xattr_writer_t.
 */

/**
 * @struct sqfs_xattr_writer_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts writing of extended attributes to a SquashFS filesystem.
 *
 * This data structure provides a simple, abstract interface to recording
 * extended attributes that hads out 32 bit tokens required for inodes to
 * refere to them.
 *
 * Use @ref sqfs_xattr_writer_begin to start a block of key-value pairs, then
 * add the individual pairs with @ref sqfs_xattr_writer_add and finaly use
 * @ref sqfs_xattr_writer_end which returns the required token.
 *
 * Finally, use @ref sqfs_xattr_writer_flush to have the extended attributes
 * written to disk.
 *
 * The writer internally takes care of propper deduplication and packaging
 * everything up in compressed meta data blocks in the multi-level hierarchy
 * used by SquashFS. See @ref sqfs_xattr_reader_t for a brief overview on how
 * SquashFS stores extended attributes.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an xattr writer instance.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * @param flags Currently must be zero or the function fails.
 *
 * @return A pointer to a new xattr writer, NULL on allocation failure.
 */
SQFS_API sqfs_xattr_writer_t *sqfs_xattr_writer_create(sqfs_u32 flags);

/**
 * @brief Begin recording a block of key-value pairs.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * Use @ref sqfs_xattr_writer_add to add the individual pairs. Call
 * @ref sqfs_xattr_writer_end when you are done.
 *
 * @param xwr A pointer to an xattr writer instance.
 * @param flags Currently must be zero, or the function will fail.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_writer_begin(sqfs_xattr_writer_t *xwr, sqfs_u32 flags);

/**
 * @brief Add a key-value pair to the current block.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * @param xwr A pointer to an xattr writer instance.
 * @param key The xattr key string.
 * @param value The associated value to store.
 * @param size The size of the value blob.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_writer_add_kv(sqfs_xattr_writer_t *xwr,
				      const char *key,
				      const void *value, size_t size);

/**
 * @brief Add a key-value struct to the current block.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * To add a key string and value blob, use @ref sqfs_xattr_writer_add_kv
 *
 * @param xwr A pointer to an xattr writer instance.
 * @param ent A pointer to a combined key-value pair.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_writer_add(sqfs_xattr_writer_t *xwr,
				   const sqfs_xattr_t *ent);

/**
 * @brief Finish a generating a key-value block.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * This function internally takes care of deduplicating the current block
 * and generates the coresponding 32 bit xattr token used by SquashFS inodes.
 * The token it returns can be one it returned previously if the block turns
 * out to be a duplicate of a previous block.
 *
 * @param xwr A pointer to an xattr writer instance.
 * @param out Returns an ID that has to be set to the inode that the block of
 *            key-value pairs belongs to.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_writer_end(sqfs_xattr_writer_t *xwr, sqfs_u32 *out);

/**
 * @brief Write all recorded key-value pairs to disk.
 *
 * @memberof sqfs_xattr_writer_t
 *
 * This function takes care of generating the extended attribute tables
 * used by SquashFS. Call it after you are donew with all other meta data
 * tables, since SquashFS requires it to be the very last thing in the
 * file system.
 *
 * @param xwr A pointer to an xattr writer instance.
 * @param file The output file to write the tables to.
 * @param super The super block to update with the table locations and flags.
 * @param cmp The compressor to user to compress the tables.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_writer_flush(const sqfs_xattr_writer_t *xwr,
				     sqfs_file_t *file, sqfs_super_t *super,
				     sqfs_compressor_t *cmp);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATRR_WRITER_H */
