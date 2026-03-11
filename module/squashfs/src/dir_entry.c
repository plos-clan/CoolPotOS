/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_entry.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/dir_entry.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

sqfs_dir_entry_t *sqfs_dir_entry_create(const char *name, sqfs_u16 mode,
					sqfs_u16 flags)
{
	sqfs_dir_entry_t *out;
	size_t len, name_len;

	if (flags & ~SQFS_DIR_ENTRY_FLAG_ALL)
		return NULL;

	name_len = strlen(name);
	if (SZ_ADD_OV(name_len, 1, &name_len))
		return NULL;
	if (SZ_ADD_OV(sizeof(*out), name_len, &len))
		return NULL;

	out = calloc(1, len);
	if (out == NULL)
		return NULL;

	out->mode = mode;
	out->flags = flags;
	memcpy(out->name, name, name_len);
	return out;
}
