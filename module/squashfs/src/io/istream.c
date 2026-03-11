/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * istream.c
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
#include <assert.h>
#include <errno.h>

#if defined(_WIN32) || defined(__WINDOWS__)
static SSIZE_T read(HANDLE fd, void *ptr, size_t size)
{
	DWORD ret;

	if (!ReadFile(fd, ptr, size, &ret, NULL)) {
		os_error_t error = get_os_error_state();
		set_os_error_state(error);

		if (error.w32_errno == ERROR_HANDLE_EOF ||
		    error.w32_errno == ERROR_BROKEN_PIPE) {
			return 0;
		}

		return -1;
	}

	return ret;
}
#endif

#define BUFSZ (131072)

typedef struct {
	sqfs_istream_t base;
	char *path;
	sqfs_file_handle_t fd;

	bool eof;
	size_t buffer_offset;
	size_t buffer_used;
	sqfs_u8 buffer[BUFSZ];
} file_istream_t;

static int precache(sqfs_istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;

	if (file->eof)
		return 0;

	if (file->buffer_offset > 0 &&
	    file->buffer_offset < file->buffer_used) {
		memmove(file->buffer, file->buffer + file->buffer_offset,
			file->buffer_used - file->buffer_offset);
	}

	file->buffer_used -= file->buffer_offset;
	file->buffer_offset = 0;

	while (file->buffer_used < BUFSZ) {
		ssize_t ret = read(file->fd, file->buffer + file->buffer_used,
				   BUFSZ - file->buffer_used);

		if (ret == 0) {
			file->eof = true;
			break;
		}

		if (ret < 0) {
#if !defined(_WIN32) && !defined(__WINDOWS__)
			if (errno == EINTR)
				continue;
#endif
			return SQFS_ERROR_IO;
		}

		file->buffer_used += ret;
	}

	return 0;
}

static int file_get_buffered_data(sqfs_istream_t *strm, const sqfs_u8 **out,
				  size_t *size, size_t want)
{
	file_istream_t *file = (file_istream_t *)strm;

	if (want > BUFSZ)
		want = BUFSZ;

	if (file->buffer_used == 0 ||
	    (file->buffer_used - file->buffer_offset) < want) {
		int ret = precache(strm);
		if (ret)
			return ret;
	}

	*out = file->buffer + file->buffer_offset;
	*size = file->buffer_used - file->buffer_offset;
	return (file->eof && *size == 0) ? 1 : 0;
}

static void file_advance_buffer(sqfs_istream_t *strm, size_t count)
{
	file_istream_t *file = (file_istream_t *)strm;

	if (count < (file->buffer_used - file->buffer_offset)) {
		file->buffer_offset += count;
	} else {
		file->buffer_offset = 0;
		file->buffer_used = 0;
	}
}

static const char *file_get_filename(sqfs_istream_t *strm)
{
	return ((file_istream_t *)strm)->path;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	sqfs_native_file_close(file->fd);
	free(file->path);
	free(file);
}

int sqfs_istream_open_handle(sqfs_istream_t **out, const char *path,
			     sqfs_file_handle_t fd, sqfs_u32 flags)
{
	file_istream_t *file;
	sqfs_istream_t *strm;
	int ret;

	if (flags & ~(SQFS_FILE_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	file = calloc(1, sizeof(*file));
	strm = (sqfs_istream_t *)file;
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

	strm->get_buffered_data = file_get_buffered_data;
	strm->advance_buffer = file_advance_buffer;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int sqfs_istream_open_file(sqfs_istream_t **out, const char *path,
			   sqfs_u32 flags)
{
	sqfs_file_handle_t fd;
	int ret;

	flags |= SQFS_FILE_OPEN_READ_ONLY;

	if (flags & (SQFS_FILE_OPEN_OVERWRITE | SQFS_FILE_OPEN_NO_SPARSE))
		return SQFS_ERROR_ARG_INVALID;

	ret = sqfs_native_file_open(&fd, path, flags);
	if (ret)
		return ret;

	ret = sqfs_istream_open_handle(out, path, fd, flags);
	if (ret != 0) {
		os_error_t err = get_os_error_state();
		sqfs_native_file_close(fd);
		set_os_error_state(err);
	}

	return ret;
}
