/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * table.h - This file is part of libsquashfs
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
#ifndef SQFS_TABLE_H
#define SQFS_TABLE_H

#include "sqfs/predef.h"

/**
 * @file table.h
 *
 * @brief Contains helper functions for reading or writing tables.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write a table to disk.
 *
 * This function takes an in-memory array, breaks it down into meta data
 * blocks, compresses and writes those blocks to the given output file and
 * then writes a raw list of 64 bit absolute locations of each meta data
 * block. The position of the location list is returned through a pointer.
 *
 * @param file The output file to write to.
 * @param cmp A compressor to use for compressing the meta data blocks.
 * @param data A pointer to a array to divide into blocks and write to disk.
 * @param table_size The size of the input array in bytes.
 * @param start Returns the absolute position of the location list.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_write_table(sqfs_file_t *file, sqfs_compressor_t *cmp,
			      const void *data, size_t table_size,
			      sqfs_u64 *start);

/**
 * @brief Read a table from a SquashFS filesystem.
 *
 * This function takes an absolute position and an array size as input. It
 * then computes the number of meta data blocks required to store this array
 * and reads that many 64 bit integers from the given start location. Each
 * integer is interpreted as the location of a meta data block containing the
 * respective array chunk.
 *
 * The entire data encoded in that way is read and uncompressed into memory.
 *
 * @param file An input file to read from.
 * @param cmp A compressor to use for uncompressing the meta data block.
 * @param table_size The size of the entire array in bytes.
 * @param location The absolute position of the location list.
 * @param lower_limit The lowest "sane" position at which to expect a meta
 *                    data block. Anything less than that is interpreted
 *                    as an out-of-bounds read.
 * @param upper_limit The highest "sane" position at which to expect a meta
 *                    data block. Anything after that is interpreted as an
 *                    out-of-bounds read.
 * @param out Returns a pointer to the table in memory.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_read_table(sqfs_file_t *file, sqfs_compressor_t *cmp,
			     size_t table_size, sqfs_u64 location,
			     sqfs_u64 lower_limit, sqfs_u64 upper_limit,
			     void **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_TABLE_H */
