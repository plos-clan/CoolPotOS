/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_unix.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "util/util.h"
#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

typedef struct {
	sqfs_dir_iterator_t base;

	struct dirent *ent;
	struct stat sb;
	dev_t device;
	int state;
	DIR *dir;
} unix_dir_iterator_t;

static void dir_destroy(sqfs_object_t *obj)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)obj;

	closedir(it->dir);
	free(it);
}

static int dir_read_link(sqfs_dir_iterator_t *base, char **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;
	ssize_t ret;
	size_t size;
	char *str;

	*out = NULL;

	if (it->state < 0)
		return it->state;

	if (it->state > 0 || it->ent == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if ((sizeof(it->sb.st_size) > sizeof(size_t)) &&
	    it->sb.st_size > SIZE_MAX) {
		return SQFS_ERROR_ALLOC;
	}

	if (SZ_ADD_OV((size_t)it->sb.st_size, 1, &size))
		return SQFS_ERROR_ALLOC;

	str = calloc(1, size);
	if (str == NULL)
		return SQFS_ERROR_ALLOC;

	ret = readlinkat(dirfd(it->dir), it->ent->d_name,
			 str, (size_t)it->sb.st_size);
	if (ret < 0) {
		free(str);
		return SQFS_ERROR_IO;
	}

	str[ret] = '\0';

	*out = str;
	return 0;
}

static int dir_next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;

	*out = NULL;
	if (it->state != 0)
		return it->state;

	errno = 0;
	it->ent = readdir(it->dir);

	if (it->ent == NULL) {
		if (errno != 0) {
			it->state = SQFS_ERROR_IO;
		} else {
			it->state = 1;
		}

		return it->state;
	}

	if (fstatat(dirfd(it->dir), it->ent->d_name,
		    &it->sb, AT_SYMLINK_NOFOLLOW)) {
		it->state = SQFS_ERROR_IO;
		return it->state;
	}

	*out = sqfs_dir_entry_create(it->ent->d_name, it->sb.st_mode, 0);
	if ((*out) == NULL) {
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	(*out)->mtime = it->sb.st_mtime;
	(*out)->dev = it->sb.st_dev;
	(*out)->rdev = it->sb.st_rdev;
	(*out)->uid = it->sb.st_uid;
	(*out)->gid = it->sb.st_gid;
	(*out)->inode = it->sb.st_ino;
	(*out)->size = it->sb.st_size;

	if ((*out)->dev != it->device)
		(*out)->flags |= SQFS_DIR_ENTRY_FLAG_MOUNT_POINT;

	return it->state;
}

static void dir_ignore_subdir(sqfs_dir_iterator_t *it)
{
	(void)it;
}

static int dir_open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;
	int fd, ret;

	*out = NULL;
	if (it->state < 0)
		return it->state;

	if (it->state > 0 || it->ent == NULL)
		return SQFS_ERROR_NO_ENTRY;

	fd = openat(dirfd(it->dir), it->ent->d_name, O_RDONLY);
	if (fd < 0)
		return SQFS_ERROR_IO;

	ret = sqfs_istream_open_handle(out, it->ent->d_name,
				       fd, SQFS_FILE_OPEN_READ_ONLY);
	if (ret != 0) {
		int err = errno;
		close(fd);
		errno = err;
	}
	return ret;
}

static int dir_read_xattr(sqfs_dir_iterator_t *it, sqfs_xattr_t **out)
{
	(void)it;
	*out = NULL;
	return 0;
}

static int create_iterator(sqfs_dir_iterator_t **out, DIR *dir);

static int dir_open_subdir(sqfs_dir_iterator_t *base, sqfs_dir_iterator_t **out)
{
	const unix_dir_iterator_t *it = (const unix_dir_iterator_t *)base;
	DIR *dir;
	int fd;

	*out = NULL;

	if (it->state < 0)
		return it->state;

	if (it->state > 0 || it->ent == NULL)
		return SQFS_ERROR_NO_ENTRY;

	fd = openat(dirfd(it->dir), it->ent->d_name, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		if (errno == ENOTDIR)
			return SQFS_ERROR_NOT_DIR;
		return SQFS_ERROR_IO;
	}

	dir = fdopendir(fd);
	if (dir == NULL) {
		int err = errno;
		close(fd);
		errno = err;
		return SQFS_ERROR_IO;
	}

	return create_iterator(out, dir);
}

static int create_iterator(sqfs_dir_iterator_t **out, DIR *dir)
{
	unix_dir_iterator_t *it = calloc(1, sizeof(*it));

	if (it == NULL) {
		closedir(dir);
		return SQFS_ERROR_ALLOC;
	}

	it->dir = dir;

	if (fstat(dirfd(dir), &it->sb)) {
		int err = errno;
		closedir(dir);
		free(it);
		errno = err;
		return SQFS_ERROR_IO;
	}

	sqfs_object_init(it, dir_destroy, NULL);
	it->device = it->sb.st_dev;
	((sqfs_dir_iterator_t *)it)->next = dir_next;
	((sqfs_dir_iterator_t *)it)->read_link = dir_read_link;
	((sqfs_dir_iterator_t *)it)->open_subdir = dir_open_subdir;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = dir_ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = dir_open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = dir_read_xattr;

	*out = (sqfs_dir_iterator_t *)it;
	return 0;
}

int sqfs_dir_iterator_create_native(sqfs_dir_iterator_t **out,
				    const char *path, sqfs_u32 flags)
{
	DIR *dir;

	*out = NULL;
	if (flags & (~SQFS_FILE_OPEN_NO_CHARSET_XFRM))
		return SQFS_ERROR_UNSUPPORTED;

	dir = opendir(path);
	if (dir == NULL)
		return SQFS_ERROR_IO;

	return create_iterator(out, dir);
}
