/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
	sqfs_ostream_t base;
	char *path;
	sqfs_u32 flags;
	sqfs_file_handle_t fd;

	sqfs_u64 sparse_count;
	sqfs_u64 size;
} file_ostream_t;

static int write_all(file_ostream_t *file, const sqfs_u8 *data, size_t size)
{
	while (size > 0) {
#if defined(_WIN32) || defined(__WINDOWS__)
		DWORD ret;
		if (!WriteFile(file->fd, data, size, &ret, NULL))
			return SQFS_ERROR_IO;
#else
		ssize_t ret = write(file->fd, data, size);

		if (ret == 0) {
			errno = EPIPE;
			return SQFS_ERROR_IO;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}
#endif
		file->size += ret;
		size -= ret;
		data += ret;
	}

	return 0;
}

static int realize_sparse(file_ostream_t *file)
{
	unsigned char *buffer;
	size_t diff, bufsz;
	int ret;

	if (file->sparse_count == 0)
		return 0;

	if (file->flags & SQFS_FILE_OPEN_NO_SPARSE) {
		bufsz = file->sparse_count > 1024 ? 1024 : file->sparse_count;
		buffer = calloc(1, bufsz);
		if (buffer == NULL)
			return SQFS_ERROR_ALLOC;

		while (file->sparse_count > 0) {
			diff = file->sparse_count > bufsz ?
				bufsz : file->sparse_count;

			ret = write_all(file, buffer, diff);
			if (ret) {
				os_error_t err = get_os_error_state();
				free(buffer);
				set_os_error_state(err);
				return ret;
			}

			file->sparse_count -= diff;
		}

		free(buffer);
	} else {
		ret = sqfs_native_file_seek(file->fd, file->sparse_count,
					    SQFS_FILE_SEEK_CURRENT |
					    SQFS_FILE_SEEK_TRUNCATE);
		if (ret)
			return ret;

		file->sparse_count = 0;
	}

	return 0;
}

static int file_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	if (size == 0 || data == NULL) {
		file->sparse_count += size;
		file->size += size;
		return 0;
	}

	ret = realize_sparse(file);
	if (ret)
		return ret;

	return write_all(file, data, size);
}

static int file_flush(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	ret = realize_sparse(file);
	if (ret)
		return ret;

#if defined(_WIN32) || defined(__WINDOWS__)
	if (!FlushFileBuffers(file->fd))
		return SQFS_ERROR_IO;
#else
	if (fsync(file->fd) != 0 && errno != EINVAL)
		return SQFS_ERROR_IO;
#endif
	return 0;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_ostream_t *file = (file_ostream_t *)obj;

	sqfs_native_file_close(file->fd);
	free(file->path);
	free(file);
}

static const char *file_get_filename(sqfs_ostream_t *strm)
{
	return ((file_ostream_t *)strm)->path;
}

int sqfs_ostream_open_handle(sqfs_ostream_t **out, const char *path,
			     sqfs_file_handle_t fd, sqfs_u32 flags)
{
	file_ostream_t *file;
	sqfs_ostream_t *strm;
	int ret;

	*out = NULL;
	if (flags & ~(SQFS_FILE_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	file = calloc(1, sizeof(*file));
	strm = (sqfs_ostream_t *)file;
	if (file == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		os_error_t err = get_os_error_state();
		free(file);
		set_os_error_state(err);
		return SQFS_ERROR_ALLOC;
	}

	ret = sqfs_native_file_duplicate(fd, &file->fd);
	if (ret) {
		os_error_t err = get_os_error_state();
		free(file->path);
		free(file);
		set_os_error_state(err);
		return ret;
	}

	sqfs_native_file_close(fd);

	file->flags = flags;
	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int sqfs_ostream_open_file(sqfs_ostream_t **out, const char *path,
			   sqfs_u32 flags)
{
	sqfs_file_handle_t fd;
	int ret;

	*out = NULL;
	if (flags & SQFS_FILE_OPEN_READ_ONLY)
		return SQFS_ERROR_ARG_INVALID;

	ret = sqfs_native_file_open(&fd, path, flags);
	if (ret)
		return ret;

	ret = sqfs_ostream_open_handle(out, path, fd, flags);
	if (ret) {
		os_error_t err = get_os_error_state();
		sqfs_native_file_close(fd);
		set_os_error_state(err);
		return ret;
	}

	return 0;
}
