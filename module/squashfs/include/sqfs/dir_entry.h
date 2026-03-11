/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_entry.h - This file is part of libsquashfs
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
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
#ifndef SQFS_DIR_ENTRY_H
#define SQFS_DIR_ENTRY_H

#include "sqfs/predef.h"

/**
 * @file dir_entry.h
 *
 * @brief Contains @ref sqfs_dir_entry_t structure representing
 *        a decoded directory entry.
 */

/**
 * @enum SQFS_DIR_ENTRY_FLAG
 *
 * @brief Additional flags for a @ref sqfs_dir_entry_t
 */
typedef enum {
	SQFS_DIR_ENTRY_FLAG_MOUNT_POINT = 0x0001,

	SQFS_DIR_ENTRY_FLAG_HARD_LINK = 0x0002,

	SQFS_DIR_ENTRY_FLAG_ALL = 0x0003,
} SQFS_DIR_ENTRY_FLAG;

/**
 * @struct sqfs_dir_entry_t
 *
 * @brief A completely decoded directory entry
 */
struct sqfs_dir_entry_t {
	/**
	 * @brief Total size of file entries
	 */
	sqfs_u64 size;

	/**
	 * @brief Unix time stamp when the entry was last modified.
	 */
	sqfs_s64 mtime;

	/**
	 * @brief Device number where the entry is stored on.
	 */
	sqfs_u64 dev;

	/**
	 * @brief Device number for device special files.
	 */
	sqfs_u64 rdev;

	/**
	 * @brief Device and filesystem specific inode number.
	 */
	sqfs_u64 inode;

	/**
	 * @brief ID of the user that owns the entry.
	 */
	sqfs_u64 uid;

	/**
	 * @brief ID of the group that owns the entry.
	 */
	sqfs_u64 gid;

	/**
	 * @brief Unix style permissions and entry type.
	 */
	sqfs_u16 mode;

	/**
	 * @brief Combination of SQFS_DIR_ENTRY_FLAG values
	 */
	sqfs_u16 flags;

	/**
	 * @brief Name of the entry
	 */
	char name[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an instance of @ref sqfs_dir_entry_t
 *
 * @memberof sqfs_dir_entry_t
 *
 * The name string is stored inline (flexible array member), the entry has
 * to be released with @ref sqfs_free
 *
 * @param name The name of the entry
 * @param mode Unix permissions and entry type
 * @param flags A combination of @ref SQFS_DIR_ENTRY_FLAG flags
 *
 * @return A pointer to a directory entry on success, NULL on failure.
 */
SQFS_API sqfs_dir_entry_t *sqfs_dir_entry_create(const char *name,
						 sqfs_u16 mode,
						 sqfs_u16 flags);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_ENTRY_H */
