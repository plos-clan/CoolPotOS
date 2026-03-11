/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * read_super.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/super.h"
#include "sqfs/error.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <string.h>

int sqfs_super_read(sqfs_super_t *super, sqfs_file_t *file)
{
	size_t block_size = 0;
	sqfs_super_t temp;
	int i, ret;

	ret = file->read_at(file, 0, &temp, sizeof(temp));
	if (ret)
		return ret;

	temp.magic = le32toh(temp.magic);
	temp.inode_count = le32toh(temp.inode_count);
	temp.modification_time = le32toh(temp.modification_time);
	temp.block_size = le32toh(temp.block_size);
	temp.fragment_entry_count = le32toh(temp.fragment_entry_count);
	temp.compression_id = le16toh(temp.compression_id);
	temp.block_log = le16toh(temp.block_log);
	temp.flags = le16toh(temp.flags);
	temp.id_count = le16toh(temp.id_count);
	temp.version_major = le16toh(temp.version_major);
	temp.version_minor = le16toh(temp.version_minor);
	temp.root_inode_ref = le64toh(temp.root_inode_ref);
	temp.bytes_used = le64toh(temp.bytes_used);
	temp.id_table_start = le64toh(temp.id_table_start);
	temp.xattr_id_table_start = le64toh(temp.xattr_id_table_start);
	temp.inode_table_start = le64toh(temp.inode_table_start);
	temp.directory_table_start = le64toh(temp.directory_table_start);
	temp.fragment_table_start = le64toh(temp.fragment_table_start);
	temp.export_table_start = le64toh(temp.export_table_start);

	if (temp.magic != SQFS_MAGIC)
		return SFQS_ERROR_SUPER_MAGIC;

	if ((temp.version_major != SQFS_VERSION_MAJOR) ||
	    (temp.version_minor != SQFS_VERSION_MINOR))
		return SFQS_ERROR_SUPER_VERSION;

	if ((temp.block_size - 1) & temp.block_size)
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	if (temp.block_size < SQFS_MIN_BLOCK_SIZE)
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	if (temp.block_size > SQFS_MAX_BLOCK_SIZE)
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	if (temp.block_log < 12 || temp.block_log > 20)
		return SQFS_ERROR_CORRUPTED;

	block_size = 1;

	for (i = 0; i < temp.block_log; ++i)
		block_size <<= 1;

	if (temp.block_size != block_size)
		return SQFS_ERROR_CORRUPTED;

	if (temp.compression_id < SQFS_COMP_MIN ||
	    temp.compression_id > SQFS_COMP_MAX)
		return SQFS_ERROR_UNSUPPORTED;

	if (temp.id_count == 0)
		return SQFS_ERROR_CORRUPTED;

	memcpy(super, &temp, sizeof(temp));
	return 0;
}
