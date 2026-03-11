/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.h - This file is part of libsquashfs
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
#ifndef SFQS_BLOCK_PROCESSOR_H
#define SFQS_BLOCK_PROCESSOR_H

#include "sqfs/predef.h"

/**
 * @file block_processor.h
 *
 * @brief Contains declarations for the data block processor.
 */

/**
 * @struct sqfs_block_processor_t
 *
 * @brief Abstracts generating of file data and fragment blocks.
 *
 * @implements sqfs_object_t
 *
 * This data structure provides a simple begin/append/end interface
 * to generate file data blocks (see @ref sqfs_block_processor_begin_file,
 * @ref sqfs_block_processor_append and @ref sqfs_block_processor_end
 * respectively).
 *
 * Internally it takes care of partitioning data in the correct block sizes,
 * adding tail-ens to fragment blocks, compressing the data, deduplicating data
 * and finally writing it to disk.
 *
 * This object is not copyable, i.e. @ref sqfs_copy will always return NULL.
 */

/**
 * @struct sqfs_block_processor_stats_t
 *
 * @brief Used to store runtime statistics about
 *        the @ref sqfs_block_processor_t.
 */
struct sqfs_block_processor_stats_t {
	/**
	 * @brief Holds the size of the structure.
	 *
	 * If a later version of libsquashfs expands this structure, the value
	 * of this field can be used to check at runtime whether the newer
	 * fields are avaialable or not.
	 */
	size_t size;

	/**
	 * @brief Total number of bytes fed into the front end API.
	 */
	sqfs_u64 input_bytes_read;

	/**
	 * @brief Total number of bytes sent down to the block processor.
	 *
	 * This is the sum of generated, compressed blocks, including blocks
	 * that were possibly deduplicated by the block writer and not
	 * counting padding that the block writer may have added.
	 */
	sqfs_u64 output_bytes_generated;

	/**
	 * @brief Total number of data blocks produced.
	 */
	sqfs_u64 data_block_count;

	/**
	 * @brief Total number of fragment blocks produced.
	 */
	sqfs_u64 frag_block_count;

	/**
	 * @brief Total number of sparse blocks encountered.
	 */
	sqfs_u64 sparse_block_count;

	/**
	 * @brief Total number of tail-end fragments produced.
	 *
	 * This number also includes the fragments that have later been
	 * eliminated by deduplication.
	 */
	sqfs_u64 total_frag_count;

	/**
	 * @brief Total number of tail-end fragments actually stored in
	 *        fragment blocks.
	 *
	 * This number does not include the fragments that have been
	 * eliminated by deduplication.
	 */
	sqfs_u64 actual_frag_count;
};

/**
 * @struct sqfs_block_processor_desc_t
 *
 * @brief Encapsulates a description for an @ref sqfs_block_processor_t
 *
 * An instance of this struct is used by @ref sqfs_block_processor_create_ex to
 * instantiate block processor objects.
 */
struct sqfs_block_processor_desc_t {
	/**
	 * @brief Holds the size of the structure.
	 *
	 * If a later version of libsquashfs expands this structure, the value
	 * of this field can be used to check at runtime whether the newer
	 * fields are avaialable or not.
	 *
	 * If @ref sqfs_block_processor_create_ex is given a struct whose size
	 * it does not recognize, it returns @ref SQFS_ERROR_ARG_INVALID.
	 */
	sqfs_u32 size;

	/**
	 * @brief The maximum size of a data block.
	 */
	sqfs_u32 max_block_size;

	/**
	 * @brief The number of worker threads to create.
	 */
	sqfs_u32 num_workers;

	/**
	 * @brief The maximum number of blocks currently in flight.
	 *
	 * When trying to add more, enqueueing blocks until the
	 * in-flight block count drops below the threshold.
	 */
	sqfs_u32 max_backlog;

	/**
	 * @brief A pointer to a compressor.
	 *
	 * If multiple worker threads are used, the deep copy function of the
	 * compressor is used to create several instances that don't interfere
	 * with each other. This means, the compressor implementation must be
	 * able to create copies of itself that can be used independendly and
	 * concurrently.
	 */
	sqfs_compressor_t *cmp;

	/**
	 * @brief A block writer to send to finished blocks to.
	 */
	sqfs_block_writer_t *wr;

	/**
	 * @brief A fragment table to use for storing block locations.
	 */
	sqfs_frag_table_t *tbl;

	/**
	 * @brief Pointer to a file to read back fragment blocks from.
	 *
	 * If file and uncmp are not NULL, the file is used to read back
	 * fragment blocks during fragment deduplication and verify possible
	 * matches. If either of them are NULL, the deduplication relies on
	 * fragment size and hash alone.
	 */
	sqfs_file_t *file;

	/**
	 * @brief A pointer to a compressor the decompresses data.
	 *
	 * @copydoc file
	 */
	sqfs_compressor_t *uncmp;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a data block processor.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param max_block_size The maximum size of a data block. Required for the
 *                       internal scratch buffer used for compressing data.
 * @param cmp A pointer to a compressor. If multiple worker threads are used,
 *            the deep copy function of the compressor is used to create
 *            several instances that don't interfere with each other.
 * @param num_workers The number of worker threads to create.
 * @param max_backlog The maximum number of blocks currently in flight. When
 *                    trying to add more, enqueueing blocks until the in-flight
 *                    block count drops below the threshold.
 * @param wr A block writer to send to finished blocks to.
 * @param tbl A fragment table to use for storing fragment and fragment block
 *            locations.
 *
 * @return A pointer to a block processor object on success, NULL on allocation
 *         failure or on failure to create and initialize the worker threads.
 */
SQFS_API
sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl);

/**
 * @brief Create a data block processor.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param desc A pointer to an extensible structure that holds the description
 *             of the block processor.
 * @param out On success, returns the pointer to the newly created block
 *            processor object.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_block_processor_create_ex(const sqfs_block_processor_desc_t *desc,
				   sqfs_block_processor_t **out);

/**
 * @brief Start writing a file.
 *
 * @memberof sqfs_block_processor_t
 *
 * After calling this function, call @ref sqfs_block_processor_append
 * repeatedly to add data to the file. Finally
 * call @ref sqfs_block_processor_end_file when you
 * are done. After writing all files, use @ref sqfs_block_processor_finish to
 * wait until all blocks that are still in flight are done and written to disk.
 *
 * The specified pointer to an inode pointer is kept internally and updated with
 * the compressed block sizes and final destinations of the file and possible
 * fragment. Since the inode may have to be grown to larger size, the actual
 * inode pointer can change over time. Furthermore, since there can still be
 * blocks in-flight even after calling @ref sqfs_block_processor_end_file, the
 * data in the inode and the value of the pointer may still change. The only
 * point at which the data writer is guaranteed to not touch them anymore is
 * after @ref sqfs_block_processor_sync or @ref sqfs_block_processor_finish has
 * returned.
 *
 * @param proc A pointer to a data writer object.
 * @param inode An optional pointer to a pointer to an inode. If not NULL, the
 *              block processor creates a file inode and stores a pointer to
 *              it here and keeps updating the inode as the file grows.
 * @param user An optional user data pointer that is passed to the
 *             the @ref sqfs_block_writer_t for each file data block.
 * @param flags A combination of @ref SQFS_BLK_FLAGS that can be used to
 *              micro manage how the data is processed.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_begin_file(sqfs_block_processor_t *proc,
					     sqfs_inode_generic_t **inode,
					     void *user, sqfs_u32 flags);

/**
 * @brief Append data to the current file.
 *
 * @memberof sqfs_block_processor_t
 *
 * Call this after @ref sqfs_block_processor_begin_file to add data to a file.
 *
 * @param proc A pointer to a data writer object.
 * @param data A pointer to a buffer to read data from.
 * @param size How many bytes should be copied out of the given
 *             buffer and written to disk.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_append(sqfs_block_processor_t *proc,
					 const void *data, size_t size);

/**
 * @brief Stop writing the current file and flush everything that is
 *        buffered internally.
 *
 * @memberof sqfs_block_processor_t
 *
 * The counter part to @ref sqfs_block_processor_begin_file.
 *
 * Even after calling this, there might still be data blocks in-flight.
 * Use @ref sqfs_block_processor_finish when you are done writing files to force
 * the remaining blocks to be processed and written to disk.
 *
 * @param proc A pointer to a data writer object.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_end_file(sqfs_block_processor_t *proc);

/**
 * @brief Submit a raw block for processing.
 *
 * @memberof sqfs_block_processor_t
 *
 * This function provides an alternative to the simple file front end to submit
 * raw data blocks to the processor. It will throw an error if called in between
 * @ref sqfs_block_processor_begin_file and @ref sqfs_block_processor_end_file.
 *
 * The flags aren't sanity checked (besides being a subset
 * of @ref SQFS_BLK_FLAGS_ALL), so in contrast to the simple file API, you can
 * shoot yourself in the foot as hard as you want.
 *
 * If not specified otherwise through flags, the fragment block/deduplication
 * and fragment consolidation are still in effect and sparse blocks are
 * discarded.
 *
 * @param proc A pointer to a block processor object.
 * @param user An optional user data pointer stored with the block and passed on
 *             to the underlying @ref sqfs_block_writer_t.
 * @param flags A combination of @ref SQFS_BLK_FLAGS that can be used to
 *              micro manage how the data is processed.
 * @param data The data to write to the block.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_submit_block(sqfs_block_processor_t *proc,
					       void *user, sqfs_u32 flags,
					       const void *data, size_t size);

/**
 * @brief Wait for the in-flight data blocks to finish.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a block processor object.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or writing to disk.
 */
SQFS_API int sqfs_block_processor_sync(sqfs_block_processor_t *proc);

/**
 * @brief Wait for the in-flight data blocks to finish and finally flush the
 *        current fragment block.
 *
 * @memberof sqfs_block_processor_t
 *
 * This does essentially the same as @ref sqfs_block_processor_sync, but after
 * syncing, it also flushes the current fragment block, even if it isn't full
 * yet and waits for it to be completed as well.
 *
 * @param proc A pointer to a block processor object.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or writing to disk.
 */
SQFS_API int sqfs_block_processor_finish(sqfs_block_processor_t *proc);

/**
 * @brief Get accumulated runtime statistics from a block processor
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a data writer object.
 *
 * @return A pointer to a @ref sqfs_block_processor_stats_t structure.
 */
SQFS_API const sqfs_block_processor_stats_t
*sqfs_block_processor_get_stats(const sqfs_block_processor_t *proc);

/**
 * @brief Create an @ref sqfs_ostream_t that writes to a data writer
 *
 * @memberof sqfs_block_processor_t
 *
 * @param out Receives a pointer to an @ref sqfs_ostream_t on success
 * @param filename A filename that the ostream returns when queried.
 * @param proc A pointer to a data writer object.
 * @param inode An optional pointer to a pointer to an inode. If not NULL, the
 *              block processor creates a file inode and stores a pointer to
 *              it here and keeps updating the inode as the file grows.
 * @param flags A combination of @ref SQFS_BLK_FLAGS that can be used to
 *              micro manage how the data is processed.
 *
 * @return Zero on sucess, a negative @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_create_ostream(sqfs_ostream_t **out,
						 const char *filename,
						 sqfs_block_processor_t *proc,
						 sqfs_inode_generic_t **inode,
						 sqfs_u32 flags);
#ifdef __cplusplus
}
#endif

#endif /* SFQS_BLOCK_PROCESSOR_H */
