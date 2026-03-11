/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * unix.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int sqfs_native_file_open(sqfs_file_handle_t *out, const char *filename,
			  sqfs_u32 flags)
{
	int open_mode;

	*out = -1;

	if (flags & ~SQFS_FILE_OPEN_ALL_FLAGS)
		return SQFS_ERROR_UNSUPPORTED;

	if (flags & SQFS_FILE_OPEN_READ_ONLY) {
		open_mode = O_RDONLY;
	} else {
		open_mode = O_CREAT | O_RDWR;

		if (flags & SQFS_FILE_OPEN_OVERWRITE) {
			open_mode |= O_TRUNC;
		} else {
			open_mode |= O_EXCL;
		}
	}

	*out = open(filename, open_mode, 0644);

	return (*out < 0) ? SQFS_ERROR_IO : 0;
}

void sqfs_native_file_close(sqfs_file_handle_t fd)
{
	while (close(fd) != 0 && errno == EINTR)
		;
}

int sqfs_native_file_duplicate(sqfs_file_handle_t in, sqfs_file_handle_t *out)
{
	*out = dup(in);
	return (*out < 0) ? SQFS_ERROR_IO : 0;
}

int sqfs_native_file_seek(sqfs_file_handle_t fd,
			  sqfs_s64 offset, sqfs_u32 flags)
{
	int whence;
	off_t off;

	switch (flags & SQFS_FILE_SEEK_TYPE_MASK) {
	case SQFS_FILE_SEEK_START:   whence = SEEK_SET; break;
	case SQFS_FILE_SEEK_CURRENT: whence = SEEK_CUR; break;
	case SQFS_FILE_SEEK_END:     whence = SEEK_END; break;
	default:                     return SQFS_ERROR_UNSUPPORTED;
	}

	if (flags & ~(SQFS_FILE_SEEK_FLAG_MASK | SQFS_FILE_SEEK_TYPE_MASK))
		return SQFS_ERROR_UNSUPPORTED;

	off = lseek(fd, offset, whence);
	if (off == ((off_t)-1)) {
		if (errno == ESPIPE)
			return SQFS_ERROR_UNSUPPORTED;
		return SQFS_ERROR_IO;
	}

	if (flags & SQFS_FILE_SEEK_TRUNCATE) {
		while (ftruncate(fd, off) != 0) {
			if (errno != EINTR)
				return SQFS_ERROR_IO;
		}
	}

	return 0;
}

int sqfs_native_file_get_size(sqfs_file_handle_t hnd, sqfs_u64 *out)
{
	struct stat sb;

	if (fstat(hnd, &sb)) {
		*out = 0;
		return SQFS_ERROR_IO;
	}

	*out = sb.st_size;
	return 0;
}
