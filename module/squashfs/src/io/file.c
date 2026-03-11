/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * file.c
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

#if !defined(_WIN32) && !defined(__WINDOWS__)
#include <unistd.h>

#define ERROR_NOT_ENOUGH_MEMORY ENOMEM
#define ERROR_NOT_SUPPORTED ENOTSUP
#define SetLastError(x) errno = (x)
#endif

typedef struct {
	sqfs_file_t base;

	bool readonly;
	sqfs_u64 size;
	sqfs_file_handle_t fd;

	char name[];
} sqfs_file_stdio_t;


static void stdio_destroy(sqfs_object_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	sqfs_native_file_close(file->fd);
	free(file);
}

static sqfs_object_t *stdio_copy(const sqfs_object_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;
	sqfs_file_stdio_t *copy;
	size_t size;

	if (!file->readonly) {
		SetLastError(ERROR_NOT_SUPPORTED);
		return NULL;
	}

	size = sizeof(*file) + strlen(file->name) + 1;
	copy = calloc(1, size);
	if (copy == NULL)
		return NULL;

	memcpy(copy, file, size);

	if (sqfs_native_file_duplicate(file->fd, &copy->fd)) {
		os_error_t error = get_os_error_state();
		free(copy);
		copy = NULL;
		set_os_error_state(error);
	}

	return (sqfs_object_t *)copy;
}

#if defined(_WIN32) || defined(__WINDOWS__)
static int stdio_read_at(sqfs_file_t *base, sqfs_u64 offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	DWORD actually_read;
	int ret;

	if (offset >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size == 0)
		return 0;

	if ((offset + size - 1) >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	ret = sqfs_native_file_seek(file->fd, offset, SQFS_FILE_SEEK_START);
	if (ret)
		return ret;

	while (size > 0) {
		if (!ReadFile(file->fd, buffer, size, &actually_read, NULL))
			return SQFS_ERROR_IO;

		size -= actually_read;
		buffer = (char *)buffer + actually_read;
	}

	return 0;
}

static int stdio_write_at(sqfs_file_t *base, sqfs_u64 offset,
			  const void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	DWORD actually_read;
	int ret;

	if (size == 0)
		return 0;

	ret = sqfs_native_file_seek(file->fd, offset, SQFS_FILE_SEEK_START);
	if (ret)
		return ret;

	while (size > 0) {
		if (!WriteFile(file->fd, buffer, size, &actually_read, NULL))
			return SQFS_ERROR_IO;

		size -= actually_read;
		buffer = (char *)buffer + actually_read;
		offset += actually_read;

		if (offset > file->size)
			file->size = offset;
	}

	return 0;
}
#else
static int stdio_read_at(sqfs_file_t *base, sqfs_u64 offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	ssize_t ret;

	while (size > 0) {
		ret = pread(file->fd, buffer, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}

		if (ret == 0)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		buffer = (char *)buffer + ret;
		size -= ret;
		offset += ret;
	}

	return 0;
}

static int stdio_write_at(sqfs_file_t *base, sqfs_u64 offset,
			  const void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	ssize_t ret;

	while (size > 0) {
		ret = pwrite(file->fd, buffer, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}

		if (ret == 0)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		buffer = (const char *)buffer + ret;
		size -= ret;
		offset += ret;
	}

	if (offset >= file->size)
		file->size = offset;

	return 0;
}
#endif

static sqfs_u64 stdio_get_size(const sqfs_file_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;

	return file->size;
}

static int stdio_truncate(sqfs_file_t *base, sqfs_u64 size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	int ret;

	ret = sqfs_native_file_seek(file->fd, size, SQFS_FILE_SEEK_START |
				    SQFS_FILE_SEEK_TRUNCATE);
	if (ret)
		return ret;

	file->size = size;
	return 0;
}

static const char *stdio_get_filename(sqfs_file_t *file)
{
	return ((sqfs_file_stdio_t *)file)->name;
}

int sqfs_file_open_handle(sqfs_file_t **out, const char *filename,
			  sqfs_file_handle_t fd, sqfs_u32 flags)
{
	sqfs_file_stdio_t *file;
	size_t size, namelen;
	sqfs_file_t *base;
	os_error_t err;
	int ret;

	*out = NULL;

	namelen = strlen(filename);
	size = sizeof(*file) + 1;

	if (SZ_ADD_OV(size, namelen, &size))
		goto fail_no_mem;

	file = calloc(1, size);
	base = (sqfs_file_t *)file;
	if (file == NULL)
		goto fail_no_mem;

	sqfs_object_init(file, stdio_destroy, stdio_copy);
	memcpy(file->name, filename, namelen);

	ret = sqfs_native_file_get_size(fd, &file->size);
	if (ret)
		goto fail_free;

	ret = sqfs_native_file_duplicate(fd, &file->fd);
	if (ret)
		goto fail_free;

	sqfs_native_file_close(fd);

	file->readonly = (flags & SQFS_FILE_OPEN_READ_ONLY) != 0;

	base->read_at = stdio_read_at;
	base->write_at = stdio_write_at;
	base->get_size = stdio_get_size;
	base->truncate = stdio_truncate;
	base->get_filename = stdio_get_filename;

	*out = base;
	return 0;
fail_free:
	err = get_os_error_state();
	free(file);
	set_os_error_state(err);
	return ret;
fail_no_mem:
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return SQFS_ERROR_ALLOC;
}

int sqfs_file_open(sqfs_file_t **out, const char *filename, sqfs_u32 flags)
{
	sqfs_file_handle_t fd;
	int ret;

	ret = sqfs_native_file_open(&fd, filename, flags);
	if (ret) {
		*out = NULL;
		return ret;
	}

	ret = sqfs_file_open_handle(out, filename, fd, flags);
	if (ret) {
		os_error_t err = get_os_error_state();
		sqfs_native_file_close(fd);
		set_os_error_state(err);
		return ret;
	}

	return 0;
}
