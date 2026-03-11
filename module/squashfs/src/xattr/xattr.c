/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * write_xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "compat.h"

#include <string.h>
#include <stdlib.h>

static const struct {
	const char *prefix;
	SQFS_XATTR_TYPE type;
} xattr_types[] = {
	{ "user.", SQFS_XATTR_USER },
	{ "trusted.", SQFS_XATTR_TRUSTED },
	{ "security.", SQFS_XATTR_SECURITY },
};

int sqfs_get_xattr_prefix_id(const char *key)
{
	size_t i, len;

	for (i = 0; i < sizeof(xattr_types) / sizeof(xattr_types[0]); ++i) {
		len = strlen(xattr_types[i].prefix);

		if (strncmp(key, xattr_types[i].prefix, len) == 0 &&
		    strlen(key) > len) {
			return xattr_types[i].type;
		}
	}

	return SQFS_ERROR_UNSUPPORTED;
}

const char *sqfs_get_xattr_prefix(SQFS_XATTR_TYPE id)
{
	size_t i;

	for (i = 0; i < sizeof(xattr_types) / sizeof(xattr_types[0]); ++i) {
		if (xattr_types[i].type == id)
			return xattr_types[i].prefix;
	}

	return NULL;
}

static sqfs_xattr_t *mkxattr(const char *prefix, const char *key,
			     const sqfs_u8 *value, size_t value_len)
{
	size_t pfx_len = (prefix == NULL) ? 0 : strlen(prefix);
	size_t key_len = strlen(key);
	sqfs_xattr_t *out;
	size_t len;

	/* len = pfx_len + (key_len + 1) + (value_len + 1) + sizeof(*out) */
	if (SZ_ADD_OV(pfx_len, key_len, &len))
		return NULL;
	if (SZ_ADD_OV(len, value_len, &len))
		return NULL;
	if (SZ_ADD_OV(len, 2, &len))
		return NULL;
	if (SZ_ADD_OV(len, sizeof(*out), &len))
		return NULL;

	out = calloc(1, len);

	if (out != NULL) {
		size_t value_offset = pfx_len + key_len + 1;

		out->key = (const char *)out->data;
		out->value = out->data + value_offset;
		out->value_len = value_len;

		if (prefix != NULL)
			memcpy(out->data, prefix, pfx_len);

		memcpy(out->data + pfx_len, key, key_len);
		memcpy(out->data + value_offset, value, value_len);
	}

	return out;
}

sqfs_xattr_t *sqfs_xattr_create(const char *key, const sqfs_u8 *value,
				size_t value_len)
{
	return mkxattr(NULL, key, value, value_len);
}

int sqfs_xattr_create_prefixed(sqfs_xattr_t **out, sqfs_u16 id,
			       const char *key, const sqfs_u8 *value,
			       size_t value_len)
{
	const char *prefix = sqfs_get_xattr_prefix(id & (~SQFS_XATTR_FLAG_OOL));

	if (prefix == NULL) {
		*out = NULL;
		return SQFS_ERROR_UNSUPPORTED;
	}

	*out = mkxattr(prefix, key, value, value_len);
	if (*out == NULL)
		return SQFS_ERROR_ALLOC;

	return 0;
}

sqfs_xattr_t *sqfs_xattr_list_copy(const sqfs_xattr_t *list)
{
	sqfs_xattr_t *copy = NULL, *copy_last = NULL;

	for (const sqfs_xattr_t *it = list; it != NULL; it = it->next) {
		sqfs_xattr_t *new = mkxattr(NULL, it->key,
					    it->value, it->value_len);
		if (new == NULL) {
			sqfs_xattr_list_free(copy);
			return NULL;
		}

		if (copy_last == NULL) {
			copy = new;
		} else {
			copy_last->next = new;
		}

		copy_last = new;
	}

	return copy;
}

void sqfs_xattr_list_free(sqfs_xattr_t *list)
{
	while (list != NULL) {
		sqfs_xattr_t *old = list;
		list = list->next;
		free(old);
	}
}
