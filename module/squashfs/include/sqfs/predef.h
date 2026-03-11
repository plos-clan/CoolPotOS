/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * predef.h - This file is part of libsquashfs
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
#ifndef SQFS_PREDEF_H
#define SQFS_PREDEF_H

/**
 * @file predef.h
 *
 * @brief Includes forward declarations of data structures,
 *        macros and integer types.
 */

#include "cp_kernel.h"

#if defined(_WIN32) || defined(__CYGWIN__)
	#if defined(SQFS_STATIC)
		#define SQFS_API
	#elif defined(SQFS_BUILDING_DLL)
		#if defined(__GNUC__) || defined(__clang__)
			#define SQFS_API __attribute__ ((dllexport))
		#else
			#define SQFS_API __declspec(dllexport)
		#endif
	#else
		#if defined(__GNUC__) || defined(__clang__)
			#define SQFS_API __attribute__ ((dllimport))
		#else
			#define SQFS_API __declspec(dllimport)
		#endif
	#endif

	#define SQFS_INTERNAL
#else
	#if defined(__GNUC__) || defined(__clang__)
		#define SQFS_API __attribute__ ((visibility ("default")))
		#define SQFS_INTERNAL  __attribute__ ((visibility ("hidden")))
	#else
		#define SQFS_API
		#define SQFS_INTERNAL
	#endif
#endif

#ifdef _MSC_VER
	#define SQFS_INLINE __forceinline
#else
	#define SQFS_INLINE __inline__ __attribute__((always_inline))
#endif

typedef uint8_t sqfs_u8;
typedef uint16_t sqfs_u16;
typedef uint32_t sqfs_u32;
typedef uint64_t sqfs_u64;

typedef int8_t sqfs_s8;
typedef int16_t sqfs_s16;
typedef int32_t sqfs_s32;
typedef int64_t sqfs_s64;

typedef struct sqfs_block_processor_t sqfs_block_processor_t;
typedef struct sqfs_compressor_config_t sqfs_compressor_config_t;
typedef struct sqfs_compressor_t sqfs_compressor_t;
typedef struct sqfs_dir_writer_t sqfs_dir_writer_t;
typedef struct sqfs_dir_reader_t sqfs_dir_reader_t;
typedef struct sqfs_dir_reader_state_t sqfs_dir_reader_state_t;
typedef struct sqfs_id_table_t sqfs_id_table_t;
typedef struct sqfs_meta_reader_t sqfs_meta_reader_t;
typedef struct sqfs_meta_writer_t sqfs_meta_writer_t;
typedef struct sqfs_xattr_reader_t sqfs_xattr_reader_t;
typedef struct sqfs_file_t sqfs_file_t;
typedef struct sqfs_tree_node_t sqfs_tree_node_t;
typedef struct sqfs_data_reader_t sqfs_data_reader_t;
typedef struct sqfs_block_hooks_t sqfs_block_hooks_t;
typedef struct sqfs_xattr_writer_t sqfs_xattr_writer_t;
typedef struct sqfs_frag_table_t sqfs_frag_table_t;
typedef struct sqfs_block_writer_t sqfs_block_writer_t;
typedef struct sqfs_block_writer_stats_t sqfs_block_writer_stats_t;
typedef struct sqfs_block_processor_stats_t sqfs_block_processor_stats_t;
typedef struct sqfs_block_processor_desc_t sqfs_block_processor_desc_t;
typedef struct sqfs_readdir_state_t sqfs_readdir_state_t;
typedef struct sqfs_xattr_t sqfs_xattr_t;
typedef struct sqfs_istream_t sqfs_istream_t;
typedef struct sqfs_ostream_t sqfs_ostream_t;
typedef struct sqfs_dir_entry_t sqfs_dir_entry_t;
typedef struct sqfs_dir_iterator_t sqfs_dir_iterator_t;

typedef struct sqfs_fragment_t sqfs_fragment_t;
typedef struct sqfs_dir_header_t sqfs_dir_header_t;
typedef struct sqfs_dir_node_t sqfs_dir_node_t;
typedef struct sqfs_dir_index_t sqfs_dir_index_t;
typedef struct sqfs_inode_t sqfs_inode_t;
typedef struct sqfs_inode_dev_t sqfs_inode_dev_t;
typedef struct sqfs_inode_dev_ext_t sqfs_inode_dev_ext_t;
typedef struct sqfs_inode_ipc_t sqfs_inode_ipc_t;
typedef struct sqfs_inode_ipc_ext_t sqfs_inode_ipc_ext_t;
typedef struct sqfs_inode_slink_t sqfs_inode_slink_t;
typedef struct sqfs_inode_slink_ext_t sqfs_inode_slink_ext_t;
typedef struct sqfs_inode_file_t sqfs_inode_file_t;
typedef struct sqfs_inode_file_ext_t sqfs_inode_file_ext_t;
typedef struct sqfs_inode_dir_t sqfs_inode_dir_t;
typedef struct sqfs_inode_dir_ext_t sqfs_inode_dir_ext_t;
typedef struct sqfs_inode_generic_t sqfs_inode_generic_t;
typedef struct sqfs_super_t sqfs_super_t;
typedef struct sqfs_xattr_entry_t sqfs_xattr_entry_t;
typedef struct sqfs_xattr_value_t sqfs_xattr_value_t;
typedef struct sqfs_xattr_id_t sqfs_xattr_id_t;
typedef struct sqfs_xattr_id_table_t sqfs_xattr_id_table_t;

/**
 * @interface sqfs_object_t
 *
 * @brief Base interface for all libsquashfs in-memory data structures.
 */
typedef struct sqfs_object_t {
	size_t refcount;

	void (*destroy)(struct sqfs_object_t *instance);

	struct sqfs_object_t *(*copy)(const struct sqfs_object_t *orig);
} sqfs_object_t;

/**
 * @brief Grab a reference to an object
 *
 * @memberof sqfs_object_t
 *
 * @param obj A pointer to an object or NULL
 *
 * @return The original pointer passed into the function
 */
static SQFS_INLINE void *sqfs_grab(void *obj)
{
	if (obj)
		((sqfs_object_t *)obj)->refcount += 1;
	return obj;
}

/**
 * @brief Drop a reference to an object, release it if it was the last reference
 *
 * @memberof sqfs_object_t
 *
 * @param obj A pointer to an object or NULL
 *
 * @return A NULL pointer.
 */
static SQFS_INLINE void *sqfs_drop(void *obj)
{
	if (obj) {
		if (((sqfs_object_t *)obj)->refcount <= 1) {
			((sqfs_object_t *)obj)->destroy((sqfs_object_t *)obj);
		} else {
			((sqfs_object_t *)obj)->refcount -= 1;
		}
	}

	return NULL;
}

/**
 * @brief Create a deep copy of an object if possible.
 *
 * @memberof sqfs_object_t
 *
 * @param obj A pointer to an object
 *
 * @return A pointer to a new object, instantiated from the old on success,
 *         NULL on failure.
 */
static SQFS_INLINE void *sqfs_copy(const void *obj)
{
	const sqfs_object_t *orig = (const sqfs_object_t *)obj;

	if (orig->copy != NULL) {
		sqfs_object_t *copy = orig->copy(orig);
		if (copy != NULL)
			copy->refcount = 1;
		return copy;
	}

	return NULL;
}

/**
 * @brief Initialize an object with default callbacks.
 *
 * @memberof sqfs_object_t
 *
 * @param obj A pointer to an uninitialized object
 */
static SQFS_INLINE
void sqfs_object_init(void *obj,
		      void (*destroy_fn)(sqfs_object_t *),
		      sqfs_object_t *(*copy_fn)(const sqfs_object_t *))
{
	((sqfs_object_t *)obj)->refcount = 1;
	((sqfs_object_t *)obj)->destroy = destroy_fn;
	((sqfs_object_t *)obj)->copy = copy_fn;
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Free a block of memory created by the squashfs library
 *
 * Some objects in the squashfs library allocate temporary chunks of memory
 * and return pointers to them, with the expectation that the user takes care
 * of freeing them memory again.
 *
 * The application cannot simply use free, as it might be linked against a
 * different runtime than the library (e.g. on systems like Windows that
 * have a huge fragmentation of runtime libraries).
 *
 * @param ptr A pointer to a memory block returned by a libsquashfs function.
 */
SQFS_API void sqfs_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_PREDEF_H */
