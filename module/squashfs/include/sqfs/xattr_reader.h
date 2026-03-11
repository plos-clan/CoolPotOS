/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_reader.h - This file is part of libsquashfs
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
#ifndef SQFS_XATTR_READER_H
#define SQFS_XATTR_READER_H

#include "sqfs/predef.h"

/**
 * @file xattr_reader.h
 *
 * @brief Contains declarations for the @ref sqfs_xattr_reader_t.
 */

/**
 * @struct sqfs_xattr_reader_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts read access to extended attributes in a SquashFS filesystem
 *
 * SquashFS stores extended attributes using multiple levels of indirection.
 * First of all, the key-value pairs of each inode (that has extended
 * attributes) are deduplicated and stored consecutively in meta data blocks.
 * Furthermore, a value can be stored out-of-band, i.e. it holds a reference to
 * another location from which the value has to be read.
 *
 * For each unique set of key-value pairs, a descriptor object is generated
 * that holds the location of the first pair, the number of pairs and the total
 * size used on disk. The array of descriptor objects is stored in multiple
 * meta data blocks. Each inode that has extended attributes holds a 32 bit
 * index into the descriptor array.
 *
 * The third layer of indirection is yet another table that points to the
 * locations of the previous two tables. Its location is in turn stored in
 * the super block.
 *
 * The sqfs_xattr_reader_t data structure takes care of the low level details
 * of loading and parsing the data.
 *
 * After creating an instance using @ref sqfs_xattr_reader_create, simply call
 * @ref sqfs_xattr_reader_load_locations to load and parse all of the location
 * tables. Then use @ref sqfs_xattr_reader_get_desc to resolve a 32 bit index
 * from an inode to a descriptor structure. Use @ref sqfs_xattr_reader_seek_kv
 * to point the reader to the start of the key-value pairs and the call
 * @ref sqfs_xattr_reader_read_key and @ref sqfs_xattr_reader_read_value
 * consecutively to read and decode each key-value pair.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an xattr reader
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function creates an object that abstracts away read only access to
 * the extended attributes in a SquashFS filesystem.
 *
 * After creating a reader and before using it, call
 * @ref sqfs_xattr_reader_load_locations to load and parse the location
 * information required to look up xattr key-value pairs.
 *
 * All pointers passed to this function are stored internally for later use.
 * Do not destroy any of the pointed to objects before destroying the xattr
 * reader.
 *
 * @param flags Currently must be set to 0, or creation will fail.
 *
 * @return A pointer to a new xattr reader instance on success, NULL on
 *         allocation failure.
 */
SQFS_API sqfs_xattr_reader_t *sqfs_xattr_reader_create(sqfs_u32 flags);

/**
 * @brief Load the locations of the xattr meta data blocks into memory
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function must be called explicitly after an xattr reader has been
 * created to load the actual location table from disk.
 *
 * @param xr A pointer to an xattr reader instance.
 * @param super A pointer to the SquashFS super block required to find the
 *              location tables.
 * @param file A pointer to a file object that contains the SquashFS filesystem.
 * @param cmp A pointer to a compressor used to uncompress the loaded meta data
 *            blocks.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_load(sqfs_xattr_reader_t *xr,
				    const sqfs_super_t *super,
				    sqfs_file_t *file, sqfs_compressor_t *cmp);

/**
 * @brief Resolve an xattr index from an inode to an xattr description
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function takes an xattr index from an extended inode type and resolves
 * it to a descriptor that points to location of the key-value pairs and
 * indicates how many key-value pairs to read from there.
 *
 * @param xr A pointer to an xattr reader instance
 * @param idx The xattr index to resolve
 * @param desc Used to return the description
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, sqfs_u32 idx,
					sqfs_xattr_id_t *desc);

/**
 * @brief Resolve an xattr index from an inode to an xattr description
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function takes an xattr descriptor object and seeks to the meta data
 * block containing the key-value pairs. The individual pairs can then be read
 * using consecutive calls to @ref sqfs_xattr_reader_read_key and
 * @ref sqfs_xattr_reader_read_value.
 *
 * @param xr A pointer to an xattr reader instance
 * @param desc The descriptor holding the location of the key-value pairs
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
				       const sqfs_xattr_id_t *desc);

/**
 * @brief Read the next xattr key
 *
 * @memberof sqfs_xattr_reader_t
 *
 * After setting the start position using @ref sqfs_xattr_reader_seek_kv, this
 * function reads and decodes an xattr key and advances the internal position
 * indicator to the location after the key. The value can then be read using
 * using @ref sqfs_xattr_reader_read_value. After reading the value, the next
 * key can be read by calling this function again.
 *
 * @param xr A pointer to an xattr reader instance
 * @param key_out Used to return the decoded key. The underlying memory can be
 *                released using a single @ref sqfs_free call.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr,
			       sqfs_xattr_entry_t **key_out);

/**
 * @brief Read the xattr value belonging to the last read key
 *
 * @memberof sqfs_xattr_reader_t
 *
 * After calling @ref sqfs_xattr_reader_read_key, this function can read and
 * decode the asociated value. The internal location indicator is then advanced
 * past the key to the next value, so @ref sqfs_xattr_reader_read_key can be
 * called again to read the next key.
 *
 * @param xr A pointer to an xattr reader instance.
 * @param key A pointer to the decoded key object.
 * @param val_out Used to return the decoded value. The underlying memory can
 *                be released using a single @ref sqfs_free call.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
				 const sqfs_xattr_entry_t *key,
				 sqfs_xattr_value_t **val_out);

/**
 * @brief Read a combined key-value pair into an @ref sqfs_xattr_t struct
 *
 * @memberof sqfs_xattr_reader_t
 *
 * @param xr A pointer to an xattr reader instance.
 * @param out Returns a pointer to a combined key-value struct.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_xattr_reader_read(sqfs_xattr_reader_t *xr, sqfs_xattr_t **out);

/**
 * @brief Read all xattrs associated with an ID into memory
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This is a convenience function that internally resolves the xattr index
 * using @ref sqfs_xattr_reader_get_desc, seeks to the key-value list using
 * @ref sqfs_xattr_reader_seek_kv and reads all the entries into a liked
 * list of @ref sqfs_xattr_t structs.
 *
 * If the index is the special value 0xFFFFFFFF, the function successfully
 * returns an empty list.
 *
 * @param xr A pointer to an xattr reader instance.
 * @param idx An xattr index.
 * @param out Returns a linked list to key-value pairs on success.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_read_all(sqfs_xattr_reader_t *xr, sqfs_u32 idx,
					sqfs_xattr_t **out);


#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_READER_H */
