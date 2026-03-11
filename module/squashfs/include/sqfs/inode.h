/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * inode.h - This file is part of libsquashfs
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
#ifndef SQFS_INODE_H
#define SQFS_INODE_H

#include "sqfs/predef.h"

/**
 * @file inode.h
 *
 * @brief Contains on-disk data structures used for inodes.
 */

/**
 * @enum SQFS_INODE_TYPE
 *
 * @brief Used by @ref sqfs_inode_t to identify the inode type.
 */
typedef enum {
	SQFS_INODE_DIR = 1,
	SQFS_INODE_FILE = 2,
	SQFS_INODE_SLINK = 3,
	SQFS_INODE_BDEV = 4,
	SQFS_INODE_CDEV = 5,
	SQFS_INODE_FIFO = 6,
	SQFS_INODE_SOCKET = 7,
	SQFS_INODE_EXT_DIR = 8,
	SQFS_INODE_EXT_FILE = 9,
	SQFS_INODE_EXT_SLINK = 10,
	SQFS_INODE_EXT_BDEV = 11,
	SQFS_INODE_EXT_CDEV = 12,
	SQFS_INODE_EXT_FIFO = 13,
	SQFS_INODE_EXT_SOCKET = 14,
} SQFS_INODE_TYPE;

/**
 * @enum SQFS_INODE_MODE
 *
 * @brief Mode bits for the @ref sqfs_inode_t mode field.
 *
 * This is basically the same that mode bits in struct stat store on Unix-like
 * systems. It is duplicated here for portability with non-POSIX platforms. In
 * case you are not familiar with Unix file permissions, a brief description
 * follows.
 *
 * There are 3 fields with permissions:
 * - of the user that owns the file
 * - of the group that the file belongs to
 * - everybody else
 *
 * Each field holds 3 bits: X, W and R meaning execute, write and read access
 * respectively. There are 2 special cases: On a directory, execute means
 * entering the directory and accessing its contents. Read and write refere
 * to reading or changing the list of entries. For symlinks, the permissions
 * are meaningless and have all bits set.
 *
 * Besides the permissions, there are 3 more bits:
 * - sticky
 * - set group id
 * - set user id
 *
 * Nowadays, the later two mean executing a program makes it run as the group
 * or user (respectively) that owns it instead of the user that actually ran
 * the program. On directories, the sticky bit means that its contents can only
 * be deleted by the actual owner, even if others have write permissions. All
 * other uses of those bits are obscure, historic and differ between flavours
 * of Unix.
 *
 * The remaining 4 bits (adding up to a total of 16) specify the type of file:
 * - named pipe, aka fifo
 * - character device
 * - directory
 * - block device
 * - regular file
 * - symlink
 * - socket
 */
typedef enum {
	SQFS_INODE_OTHERS_X = 00001,
	SQFS_INODE_OTHERS_W = 00002,
	SQFS_INODE_OTHERS_R = 00004,
	SQFS_INODE_OTHERS_MASK = 00007,

	SQFS_INODE_GROUP_X = 00010,
	SQFS_INODE_GROUP_W = 00020,
	SQFS_INODE_GROUP_R = 00040,
	SQFS_INODE_GROUP_MASK = 00070,

	SQFS_INODE_OWNER_X = 00100,
	SQFS_INODE_OWNER_W = 00200,
	SQFS_INODE_OWNER_R = 00400,
	SQFS_INODE_OWNER_MASK = 00700,

	SQFS_INODE_STICKY = 01000,
	SQFS_INODE_SET_GID = 02000,
	SQFS_INODE_SET_UID = 04000,

	SQFS_INODE_MODE_FIFO = 0010000,
	SQFS_INODE_MODE_CHR = 0020000,
	SQFS_INODE_MODE_DIR = 0040000,
	SQFS_INODE_MODE_BLK = 0060000,
	SQFS_INODE_MODE_REG = 0100000,
	SQFS_INODE_MODE_LNK = 0120000,
	SQFS_INODE_MODE_SOCK = 0140000,
	SQFS_INODE_MODE_MASK = 0170000,
} SQFS_INODE_MODE;

/**
 * @struct sqfs_inode_t
 *
 * @brief Common inode structure
 *
 * This structure holds the fields common for all inodes. Depending on the type
 * field, a specific inode structure follows.
 */
struct sqfs_inode_t {
	/**
	 * @brief An @ref SQFS_INODE_TYPE value.
	 */
	sqfs_u16 type;

	/**
	 * @brief Mode filed holding permission bits only. The type is derived
	 *        from the type field.
	 *
	 * This field holds a combination of @ref SQFS_INODE_MODE flags.
	 */
	sqfs_u16 mode;

	/**
	 * @brief An index into the ID table where the owner UID is located.
	 */
	sqfs_u16 uid_idx;

	/**
	 * @brief An index into the ID table where the owner GID is located.
	 */
	sqfs_u16 gid_idx;

	/**
	 * @brief Last modifcation time.
	 *
	 * This field counts seconds (not counting leap seconds) since 00:00,
	 * Jan 1 1970 UTC. This field is unsigned, so it expires in the year
	 * 2106 (as opposed to 2038).
	 */
	sqfs_u32 mod_time;

	/**
	 * @brief Unique inode number
	 */
	sqfs_u32 inode_number;
};

/**
 * @struct sqfs_inode_dev_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_BDEV
 *        or @ref SQFS_INODE_CDEV.
 */
struct sqfs_inode_dev_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Device number.
	 */
	sqfs_u32 devno;
};

/**
 * @struct sqfs_inode_dev_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_BDEV
 *        or @ref SQFS_INODE_EXT_CDEV.
 */
struct sqfs_inode_dev_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Device number.
	 */
	sqfs_u32 devno;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
};

/**
 * @struct sqfs_inode_ipc_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_FIFO
 *        or @ref SQFS_INODE_SOCKET.
 */
struct sqfs_inode_ipc_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;
};

/**
 * @struct sqfs_inode_ipc_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_FIFO
 *        or @ref SQFS_INODE_EXT_SOCKET.
 */
struct sqfs_inode_ipc_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
};

/**
 * @struct sqfs_inode_slink_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_SLINK.
 *
 * The declaration does not contain the flexible array member of the symlink
 * target because @ref sqfs_inode_generic_t would otherwies be impossible to
 * implement without violating the C standard.
 */
struct sqfs_inode_slink_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	sqfs_u32 target_size;

	/*sqfs_u8 target[];*/
};

/**
 * @struct sqfs_inode_slink_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_SLINK.
 *
 * The declaration does not contain the flexible array member of the symlink
 * target because it is wedged right in between the target size and the xattr
 * identifier.
 */
struct sqfs_inode_slink_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	sqfs_u32 target_size;

	/*sqfs_u8 target[];*/

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
};

/**
 * @struct sqfs_inode_file_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_FILE.
 *
 * The declaration does not contain the flexible array member for the data
 * block sizes because @ref sqfs_inode_generic_t would otherwies be impossible
 * to implement without violating the C standard.
 *
 * For each data block, the inode is followed by a 32 bit integer that holds
 * the on-disk size of the compressed block in bytes and has bit number 24
 * set if the block is stored uncompressed.
 *
 * If a block size is specified as zero, it is assumed to be an entire block
 * filled with zero bytes.
 */
struct sqfs_inode_file_t {
	/**
	 * @brief Absolute position of the first data block.
	 */
	sqfs_u32 blocks_start;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	sqfs_u32 fragment_index;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	sqfs_u32 fragment_offset;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	sqfs_u32 file_size;

	/*sqfs_u32 block_sizes[];*/
};

/**
 * @struct sqfs_inode_file_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_FILE.
 *
 * @copydoc sqfs_inode_file_t
 */
struct sqfs_inode_file_ext_t {
	/**
	 * @brief Absolute position of the first data block.
	 */
	sqfs_u64 blocks_start;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	sqfs_u64 file_size;

	/**
	 * @brief If the file is sparse, holds the number of bytes not written
	 *        to disk because of the omitted sparse blocks.
	 */
	sqfs_u64 sparse;

	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	sqfs_u32 fragment_idx;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	sqfs_u32 fragment_offset;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;

	/*sqfs_u32 block_sizes[];*/
};

/**
 * @struct sqfs_inode_dir_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_DIR.
 */
struct sqfs_inode_dir_t {
	/**
	 * @brief Offset from the directory table start to the location of the
	 *        meta data block containing the first directory header.
	 */
	sqfs_u32 start_block;

	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Combined size of all directory entries and headers in bytes.
	 *
	 * The value stored here is off by 3 bytes, i.e. 3 bytes larger than
	 * the actual listing on disk. The Linux implementation of SquashFS
	 * uses readdir() offsets 0 and 1 to synthesize "." and ".." entries,
	 * and after that looks into the actual directory.
	 *
	 * Why store an off-by-3 value on disk instead of faking it in the
	 * kernel and have something consistent here? Beats me, but that's
	 * how it is implemented.
	 */
	sqfs_u16 size;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	sqfs_u16 offset;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	sqfs_u32 parent_inode;
};

/**
 * @struct sqfs_inode_dir_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_DIR.
 */
struct sqfs_inode_dir_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Size of all directory entries and headers in bytes plus 3.
	 */
	sqfs_u32 size;

	/**
	 * @brief Offset from the directory table start to the location of the
	 *        meta data block containing the first directory header.
	 */
	sqfs_u32 start_block;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	sqfs_u32 parent_inode;

	/**
	 * @brief Number of directory index entries following the inode
	 *
	 * This number counts the number of @ref sqfs_dir_index_t entries
	 * following the inode.
	 */
	sqfs_u16 inodex_count;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	sqfs_u16 offset;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
};

/**
 * @struct sqfs_inode_generic_t
 *
 * @brief A generic inode structure that combines all others and provides
 *        additional information.
 *
 * A few helper functions exist for working with this. For instance,
 * @ref sqfs_meta_reader_read_inode can read an inode from disk and assemble it
 * into an instance of this structure. Similarly, the
 * @ref sqfs_meta_writer_write_inode function can break it down into encoded,
 * on-disk structures and write them to disk.
 */
struct sqfs_inode_generic_t {
	/**
	 * @brief The common fields for all inodes.
	 */
	sqfs_inode_t base;

	/**
	 * @brief Maximum number of available data bytes in the payload.
	 *
	 * This is used for dynamically growing an inode. The actual number
	 * of used payload bytes is stored in @ref payload_bytes_used.
	 */
	sqfs_u32 payload_bytes_available;

	/**
	 * @brief Number of used data bytes in the payload.
	 *
	 * For file inodes, stores the number of blocks used. For extended
	 * directory inodes, stores the number of payload bytes following
	 * for the directory index.
	 */
	sqfs_u32 payload_bytes_used;

	/**
	 * @brief Type specific inode data.
	 */
	union {
		sqfs_inode_dev_t dev;
		sqfs_inode_dev_ext_t dev_ext;
		sqfs_inode_ipc_t ipc;
		sqfs_inode_ipc_ext_t ipc_ext;
		sqfs_inode_slink_t slink;
		sqfs_inode_slink_ext_t slink_ext;
		sqfs_inode_file_t file;
		sqfs_inode_file_ext_t file_ext;
		sqfs_inode_dir_t dir;
		sqfs_inode_dir_ext_t dir_ext;
	} data;

	/**
	 * @brief Holds type specific extra data, such as symlink target.
	 *
	 * For regular file inodes, this is an array of block sizes. For symlink
	 * inodes, this is actually a string holding the target. For extended
	 * directory inodes, this is actually a blob of tightly packed directory
	 * index entries.
	 */
	sqfs_u32 extra[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the number of file blocks in a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * @param inode A pointer to an inode.
 *
 * @return The number of blocks.
 */
static SQFS_INLINE
size_t sqfs_inode_get_file_block_count(const sqfs_inode_generic_t *inode)
{
	return inode->payload_bytes_used / sizeof(sqfs_u32);
}

/**
 * @brief Get the extended attribute index of an inode
 *
 * @memberof sqfs_inode_generic_t
 *
 * For basic inodes, this returns the inode index 0xFFFFFFFF, i.e. the
 * sentinel value indicating that there are no xattrs.
 *
 * @param inode A pointer to an inode.
 * @param out Returns the extended attribute index on success.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_get_xattr_index(const sqfs_inode_generic_t *inode,
					sqfs_u32 *out);

/**
 * @brief Set the extended attribute index of an inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * For basic inodes, this function promotes the inodes to extended inodes if
 * the index is not 0xFFFFFFFF. The function does not try to demote extended
 * inodes if the index is 0xFFFFFFFF, because that would cause additional
 * information like a directory index to be lost.
 *
 * @param inode A pointer to an inode.
 * @param index The extended attribute index.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_set_xattr_index(sqfs_inode_generic_t *inode,
					sqfs_u32 index);

/**
 * @brief Convert a basic inode to an extended inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * For inodes that already have an extended type, this is a no-op.
 *
 * @param inode A pointer to an inode.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_make_extended(sqfs_inode_generic_t *inode);

/**
 * @brief Convert an extended inode to a basic inode if possible.
 *
 * @memberof sqfs_inode_generic_t
 *
 * For inodes that already have a basic type, this is a no-op. If the inode
 * has values set that the coresponding basic type doesn't support (e.g. it
 * has an xattr index set or a regular file which requires 64 bit size
 * counter), it is left as an extended type and success state is returned.
 *
 * @param inode A pointer to an inode.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_make_basic(sqfs_inode_generic_t *inode);

/**
 * @brief Update the file size of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * If the new size is wider than 32 bit, a basic file inode is transparently
 * promoted to an extended file inode. For extended inodes, if the new size
 * is small enough and was the only requirement for the extended type, the
 * node is transparently demoted to a basic file inode.
 *
 * @param inode A pointer to an inode.
 * @param size The new size to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_file_size(sqfs_inode_generic_t *inode,
				      sqfs_u64 size);

/**
 * @brief Update the location of the first data block of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * If the new location is wider than 32 bit, a basic file inode is
 * transparently promoted to an extended file inode. For extended inodes,
 * if the new size is small enough and was the only requirement for the
 * extended type, the node is transparently demoted to a basic file inode.
 *
 * @param inode A pointer to an inode.
 * @param location The new location to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_file_block_start(sqfs_inode_generic_t *inode,
					     sqfs_u64 location);

/**
 * @brief Update the file fragment location of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * @param inode A pointer to an inode.
 * @param index The new fragment index to set.
 * @param offset The new fragment offset to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_frag_location(sqfs_inode_generic_t *inode,
					  sqfs_u32 index, sqfs_u32 offset);

/**
 * @brief Get the file size of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * @param inode A pointer to an inode.
 * @param size Returns the file size.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_file_size(const sqfs_inode_generic_t *inode,
				      sqfs_u64 *size);

/**
 * @brief Get the file fragment location of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * @param inode A pointer to an inode.
 * @param index Returns the fragment index.
 * @param offset Returns the fragment offset.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_frag_location(const sqfs_inode_generic_t *inode,
					  sqfs_u32 *index, sqfs_u32 *offset);

/**
 * @brief Get the location of the first data block of a regular file inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * @param inode A pointer to an inode.
 * @param location Returns the location.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_file_block_start(const sqfs_inode_generic_t *inode,
					     sqfs_u64 *location);

/**
 * @brief Unpack the a directory index structure from an inode.
 *
 * @memberof sqfs_inode_generic_t
 *
 * The generic inode contains in its payload the raw directory index (with
 * bytes swapped to host enian), but still with single byte alignment. This
 * function seeks through the blob using an integer index (not offset) and
 * fiddles the raw data out into a propperly aligned, external structure.
 *
 * @param inode A pointer to an inode.
 * @param out Returns the index entry. Can be freed with a single
 *            @ref sqfs_free call.
 * @param index An index value between 0 and inodex_count.
 *
 * @return Zero on success, @ref SQFS_ERROR_OUT_OF_BOUNDS if the given index
 *         points outside the directory index. Can return other error codes
 *         (e.g. if the inode isn't a directory or allocation failed).
 */
SQFS_API
int sqfs_inode_unpack_dir_index_entry(const sqfs_inode_generic_t *inode,
				      sqfs_dir_index_t **out,
				      size_t index);

/**
 * @brief Create a directory entry from an inode
 *
 * @param name The file name or path to store in the directory entry
 * @param len The lengh of the file name, or 0 to use strlen internally
 * @param inode The inode from which to use the data
 * @param idtbl An ID table to use for resolving the inodes uid & gid
 * @param out Returns a pointer to a @ref sqfs_dir_entry_t on success
 *
 * @return Zero on success, a negative @ref SQFS_ERROR value on failure
 */
SQFS_API
int sqfs_dir_entry_from_inode(const char *name, size_t len,
			      const sqfs_inode_generic_t *inode,
			      const sqfs_id_table_t *idtbl,
			      sqfs_dir_entry_t **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_INODE_H */
