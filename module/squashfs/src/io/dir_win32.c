/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_win32.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "util/util.h"
#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define UNIX_EPOCH_ON_W32 11644473600UL
#define W32_TICS_PER_SEC 10000000UL

typedef struct {
	sqfs_dir_iterator_t base;

	WIN32_FIND_DATAW ent;
	HANDLE dirhnd;
	int state;
	bool is_first;

	WCHAR path[];
} dir_iterator_win32_t;

static sqfs_s64 w32time_to_unix(const FILETIME *ft)
{
	sqfs_u64 w32ts;

	w32ts = ft->dwHighDateTime;
	w32ts <<= 32UL;
	w32ts |= ft->dwLowDateTime;

	w32ts /= W32_TICS_PER_SEC;

	if (w32ts <= UNIX_EPOCH_ON_W32)
		return -((sqfs_s64)(UNIX_EPOCH_ON_W32 - w32ts));

	return w32ts - UNIX_EPOCH_ON_W32;
}

static int dir_iterator_read_link(sqfs_dir_iterator_t *it, char **out)
{
	(void)it;
	*out = NULL;
	return SQFS_ERROR_UNSUPPORTED;
}

static int dir_iterator_next(sqfs_dir_iterator_t *it, sqfs_dir_entry_t **out)
{
	dir_iterator_win32_t *w32 = (dir_iterator_win32_t *)it;
	sqfs_dir_entry_t *ent = NULL;
	DWORD length;

	if (w32->state == 0 && !w32->is_first) {
		if (!FindNextFileW(w32->dirhnd, &w32->ent)) {
			os_error_t err = get_os_error_state();

			if (err.w32_errno == ERROR_NO_MORE_FILES) {
				w32->state = 1;
			} else {
				w32->state = SQFS_ERROR_IO;
			}

			set_os_error_state(err);
		}
	}

	w32->is_first = false;

	if (w32->state != 0)
		goto out;

	length = WideCharToMultiByte(CP_UTF8, 0, w32->ent.cFileName,
				     -1, NULL, 0, NULL, NULL);
	if (length <= 0) {
		w32->state = SQFS_ERROR_ALLOC;
		goto out;
	}

	ent = alloc_flex(sizeof(*ent), 1, length + 1);
	if (ent == NULL) {
		w32->state = SQFS_ERROR_ALLOC;
		goto out;
	}

	WideCharToMultiByte(CP_UTF8, 0, w32->ent.cFileName, -1,
			    ent->name, length + 1, NULL, NULL);

	if (w32->ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		ent->mode = S_IFDIR | 0755;
	} else {
		ent->mode = S_IFREG | 0644;
		ent->size = w32->ent.nFileSizeHigh;
		ent->size <<= 32UL;
		ent->size |= w32->ent.nFileSizeLow;
	}

	ent->mtime = w32time_to_unix(&(w32->ent.ftLastWriteTime));
out:
	*out = ent;
	return w32->state;
}

static void dir_iterator_destroy(sqfs_object_t *obj)
{
	dir_iterator_win32_t *dir = (dir_iterator_win32_t *)obj;

	FindClose(dir->dirhnd);
	free(dir);
}

static void dir_iterator_ignore_subdir(sqfs_dir_iterator_t *it)
{
	(void)it;
}

static int dir_iterator_open_file_ro(sqfs_dir_iterator_t *it,
				     sqfs_istream_t **out)
{
	dir_iterator_win32_t *dir = (dir_iterator_win32_t *)it;
	size_t plen, slen;
	os_error_t err;
	WCHAR *str16;
	DWORD length;
	HANDLE hnd;
	char *str8;
	int ret;

	*out = NULL;

	if (dir->state != 0)
		return (dir->state > 0) ? SQFS_ERROR_NO_ENTRY : dir->state;

	plen = wcslen(dir->path) - 1;
	slen = wcslen(dir->ent.cFileName);

	str16 = calloc(plen + slen + 1, sizeof(WCHAR));
	if (str16 == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(str16,        dir->path,          plen * sizeof(WCHAR));
	memcpy(str16 + plen, dir->ent.cFileName, slen * sizeof(WCHAR));

	hnd = CreateFileW(str16, GENERIC_READ, FILE_SHARE_READ, NULL,
			  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			  NULL);

	err = get_os_error_state();
	free(str16);
	set_os_error_state(err);

	if (hnd == INVALID_HANDLE_VALUE)
		return SQFS_ERROR_IO;

	length = WideCharToMultiByte(CP_UTF8, 0, dir->ent.cFileName,
				     -1, NULL, 0, NULL, NULL);
	if (length <= 0) {
		CloseHandle(hnd);
		return SQFS_ERROR_ALLOC;
	}

	str8 = calloc(1, length + 1);
	if (str8 == NULL) {
		CloseHandle(hnd);
		return SQFS_ERROR_ALLOC;
	}

	WideCharToMultiByte(CP_UTF8, 0, dir->ent.cFileName, -1, str8,
			    length + 1, NULL, NULL);

	ret = sqfs_istream_open_handle(out, str8, hnd,
				       SQFS_FILE_OPEN_READ_ONLY);
	err = get_os_error_state();
	free(str8);
	if (ret != 0)
		CloseHandle(hnd);
	set_os_error_state(err);

	return ret;
}

static int dir_iterator_read_xattr(sqfs_dir_iterator_t *it, sqfs_xattr_t **out)
{
	(void)it;
	*out = NULL;
	return 0;
}

static int dir_iterator_init(dir_iterator_win32_t *it);

static int dir_iterator_open_subdir(sqfs_dir_iterator_t *it,
				    sqfs_dir_iterator_t **out)
{
	const dir_iterator_win32_t *dir = (const dir_iterator_win32_t *)it;
	dir_iterator_win32_t *sub = NULL;
	size_t plen, slen, total;
	int ret;

	*out = NULL;

	if (dir->state != 0)
		return (dir->state > 0) ? SQFS_ERROR_NO_ENTRY : dir->state;

	if (!(dir->ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return SQFS_ERROR_NOT_DIR;

	plen = wcslen(dir->path) - 1;
	slen = wcslen(dir->ent.cFileName);
	total = plen + slen + 3;

	sub = alloc_flex(sizeof(*sub), sizeof(WCHAR), total);
	if (sub == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(sub->path, dir->path, plen * sizeof(WCHAR));
	memcpy(sub->path + plen, dir->ent.cFileName, slen * sizeof(WCHAR));
	sub->path[plen + slen    ] = '\\';
	sub->path[plen + slen + 1] = '*';
	sub->path[plen + slen + 2] = '\0';

	ret = dir_iterator_init(sub);
	if (ret != 0) {
		os_error_t err = get_os_error_state();
		free(sub);
		sub = NULL;
		set_os_error_state(err);
	}

	*out = (sqfs_dir_iterator_t *)sub;
	return ret;
}

static int dir_iterator_init(dir_iterator_win32_t *it)
{
	sqfs_object_init(it, dir_iterator_destroy, NULL);

	((sqfs_dir_iterator_t *)it)->next = dir_iterator_next;
	((sqfs_dir_iterator_t *)it)->read_link = dir_iterator_read_link;
	((sqfs_dir_iterator_t *)it)->open_subdir = dir_iterator_open_subdir;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = dir_iterator_ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = dir_iterator_open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = dir_iterator_read_xattr;
	it->is_first = true;
	it->state = 0;

	it->dirhnd = FindFirstFileW(it->path, &it->ent);
	if (it->dirhnd == INVALID_HANDLE_VALUE)
		return SQFS_ERROR_IO;

	return 0;
}

int sqfs_dir_iterator_create_native(sqfs_dir_iterator_t **out,
				    const char *path, sqfs_u32 flags)
{
	dir_iterator_win32_t *it;
	WCHAR *wpath = NULL;
	void *new = NULL;
	DWORD length;
	size_t len;
	int ret;

	*out = NULL;
	if (flags & (~SQFS_FILE_OPEN_NO_CHARSET_XFRM))
		return SQFS_ERROR_UNSUPPORTED;

	/* convert path to UTF-16, append "\\*" */
	len = strlen(path);
	while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\'))
		--len;

	length = MultiByteToWideChar(CP_UTF8, 0, path, len, NULL, 0);
	if (length <= 0)
		return SQFS_ERROR_ALLOC;

	wpath = calloc(sizeof(wpath[0]), length + 3);
	if (wpath == NULL)
		return SQFS_ERROR_ALLOC;

	MultiByteToWideChar(CP_UTF8, 0, path, len, wpath, length + 1);

	for (DWORD i = 0; i < length; ++i) {
		if (wpath[i] == '/')
			wpath[i] = '\\';
	}

	wpath[length++] = '\\';
	wpath[length++] = '*';
	wpath[length++] = '\0';

	/* create the sourrounding iterator structure */
	new = realloc(wpath, sizeof(*it) + length * sizeof(wpath[0]));
	if (new == NULL) {
		free(wpath);
		return SQFS_ERROR_ALLOC;
	}

	it = new;
	wpath = NULL;
	memmove(it->path, new, length * sizeof(wpath[0]));

	/* initialize */
	memset(it, 0, offsetof(dir_iterator_win32_t, path));
	ret = dir_iterator_init(it);
	if (ret != 0) {
		os_error_t err = get_os_error_state();
		free(it);
		it = NULL;
		set_os_error_state(err);
	}

	*out = (sqfs_dir_iterator_t *)it;
	return ret;
}
