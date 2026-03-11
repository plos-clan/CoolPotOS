/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * win32.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"
#include "compat.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int sqfs_native_file_open(sqfs_file_handle_t *out, const char *filename,
			  sqfs_u32 flags)
{
	int access_flags, creation_mode, share_mode;
	WCHAR *wpath = NULL;
	os_error_t err;
	DWORD length;

	*out = INVALID_HANDLE_VALUE;

	if (flags & ~SQFS_FILE_OPEN_ALL_FLAGS) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (flags & SQFS_FILE_OPEN_READ_ONLY) {
		access_flags = GENERIC_READ;
		creation_mode = OPEN_EXISTING;
		share_mode = FILE_SHARE_READ;
	} else {
		access_flags = GENERIC_READ | GENERIC_WRITE;
		share_mode = 0;

		if (flags & SQFS_FILE_OPEN_OVERWRITE) {
			creation_mode = CREATE_ALWAYS;
		} else {
			creation_mode = CREATE_NEW;
		}
	}

	if (flags & SQFS_FILE_OPEN_NO_CHARSET_XFRM) {
		*out = CreateFileA(filename, access_flags, share_mode, NULL,
				   creation_mode, FILE_ATTRIBUTE_NORMAL, NULL);
	} else {
		length = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
		if (length <= 0)
			return SQFS_ERROR_INTERNAL;

		wpath = calloc(sizeof(wpath[0]), length + 1);
		if (wpath == NULL) {
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return SQFS_ERROR_ALLOC;
		}

		MultiByteToWideChar(CP_UTF8, 0, filename, -1,
				    wpath, length + 1);
		wpath[length] = '\0';

		*out = CreateFileW(wpath, access_flags, share_mode, NULL,
				   creation_mode, FILE_ATTRIBUTE_NORMAL,
				   NULL);

		err = get_os_error_state();
		free(wpath);
		set_os_error_state(err);
	}

	return (*out == INVALID_HANDLE_VALUE) ? SQFS_ERROR_IO : 0;
}

void sqfs_native_file_close(sqfs_file_handle_t hnd)
{
	CloseHandle(hnd);
}

int sqfs_native_file_duplicate(sqfs_file_handle_t in, sqfs_file_handle_t *out)
{
	HANDLE hProc = GetCurrentProcess();
	BOOL ret = DuplicateHandle(hProc, in, hProc, out, 0,
				   FALSE, DUPLICATE_SAME_ACCESS);

	return ret ? 0 : SQFS_ERROR_IO;
}

int sqfs_native_file_seek(sqfs_file_handle_t fd,
			  sqfs_s64 offset, sqfs_u32 flags)
{
	LARGE_INTEGER pos;
	DWORD whence;

	switch (flags & SQFS_FILE_SEEK_TYPE_MASK) {
	case SQFS_FILE_SEEK_START:   whence = FILE_BEGIN; break;
	case SQFS_FILE_SEEK_CURRENT: whence = FILE_CURRENT; break;
	case SQFS_FILE_SEEK_END:     whence = FILE_END; break;
	default:                     return SQFS_ERROR_UNSUPPORTED;
	}

	if (flags & ~(SQFS_FILE_SEEK_FLAG_MASK | SQFS_FILE_SEEK_TYPE_MASK))
		return SQFS_ERROR_UNSUPPORTED;

	if (GetFileType(fd) != FILE_TYPE_DISK)
		return SQFS_ERROR_UNSUPPORTED;

	pos.QuadPart = offset;
	if (!SetFilePointerEx(fd, pos, NULL, whence))
		return SQFS_ERROR_IO;

	if (flags & SQFS_FILE_SEEK_TRUNCATE) {
		if (!SetEndOfFile(fd))
			return SQFS_ERROR_IO;
	}

	return 0;
}

int sqfs_native_file_get_size(sqfs_file_handle_t hnd, sqfs_u64 *out)
{
	LARGE_INTEGER size;

	if (!GetFileSizeEx(hnd, &size)) {
		*out = 0;
		return SQFS_ERROR_IO;
	}

	*out = size.QuadPart;
	return 0;
}
