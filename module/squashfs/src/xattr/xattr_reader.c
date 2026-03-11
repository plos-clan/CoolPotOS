/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/xattr_reader.h"
#include "sqfs/meta_reader.h"
#include "sqfs/super.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct sqfs_xattr_reader_t {
	sqfs_object_t base;

	sqfs_u64 xattr_start;
	sqfs_u64 xattr_end;

	size_t num_id_blocks;
	size_t num_ids;

	sqfs_u64 *id_block_starts;

	sqfs_meta_reader_t *idrd;
	sqfs_meta_reader_t *kvrd;
};

static sqfs_object_t *xattr_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_xattr_reader_t *xr = (const sqfs_xattr_reader_t *)obj;
	sqfs_xattr_reader_t *copy = malloc(sizeof(*copy));

	if (copy == NULL)
		return NULL;

	memcpy(copy, xr, sizeof(*xr));

	if (xr->kvrd != NULL) {
		copy->kvrd = sqfs_copy(xr->kvrd);
		if (copy->kvrd == NULL)
			goto fail;
	}

	if (xr->idrd != NULL) {
		copy->idrd = sqfs_copy(xr->idrd);
		if (copy->idrd == NULL)
			goto fail;
	}

	if (xr->id_block_starts != NULL) {
		copy->id_block_starts = alloc_array(sizeof(sqfs_u64),
						    xr->num_id_blocks);
		if (copy->id_block_starts == NULL)
			goto fail;

		memcpy(copy->id_block_starts, xr->id_block_starts,
		       sizeof(sqfs_u64) * xr->num_id_blocks);
	}

	return (sqfs_object_t *)copy;
fail:
	sqfs_drop(copy->idrd);
	sqfs_drop(copy->kvrd);
	free(copy);
	return NULL;
}

static void xattr_reader_destroy(sqfs_object_t *obj)
{
	sqfs_xattr_reader_t *xr = (sqfs_xattr_reader_t *)obj;

	sqfs_drop(xr->kvrd);
	sqfs_drop(xr->idrd);
	free(xr->id_block_starts);
	free(xr);
}

int sqfs_xattr_reader_load(sqfs_xattr_reader_t *xr, const sqfs_super_t *super,
			   sqfs_file_t *file, sqfs_compressor_t *cmp)
{
	sqfs_xattr_id_table_t idtbl;
	size_t i;
	int err;

	/* sanity check */
	if (super->flags & SQFS_FLAG_NO_XATTRS)
		return 0;

	if (super->xattr_id_table_start == 0xFFFFFFFFFFFFFFFF)
		return 0;

	if (super->xattr_id_table_start >= super->bytes_used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	/* cleanup pre-existing data */
	xr->idrd = sqfs_drop(xr->idrd);
	xr->kvrd = sqfs_drop(xr->kvrd);

	free(xr->id_block_starts);
	xr->id_block_starts = NULL;

	/* read the locations table */
	err = file->read_at(file, super->xattr_id_table_start,
			    &idtbl, sizeof(idtbl));
	if (err)
		return err;

	xr->xattr_start = le64toh(idtbl.xattr_table_start);
	xr->num_ids = le32toh(idtbl.xattr_ids);
	xr->num_id_blocks =
		(xr->num_ids * sizeof(sqfs_xattr_id_t)) / SQFS_META_BLOCK_SIZE;

	if ((xr->num_ids * sizeof(sqfs_xattr_id_t)) % SQFS_META_BLOCK_SIZE)
		xr->num_id_blocks += 1;

	xr->id_block_starts = alloc_array(sizeof(sqfs_u64), xr->num_id_blocks);
	if (xr->id_block_starts == NULL) {
		if (errno == EOVERFLOW)
			return SQFS_ERROR_OVERFLOW;
		return SQFS_ERROR_ALLOC;
	}

	err = file->read_at(file, super->xattr_id_table_start + sizeof(idtbl),
			    xr->id_block_starts,
			    sizeof(sqfs_u64) * xr->num_id_blocks);
	if (err)
		goto fail_blocks;

	for (i = 0; i < xr->num_id_blocks; ++i) {
		xr->id_block_starts[i] = le64toh(xr->id_block_starts[i]);

		if (xr->id_block_starts[i] > super->bytes_used) {
			err = SQFS_ERROR_OUT_OF_BOUNDS;
			goto fail_blocks;
		}
	}

	/* create the meta data readers */
	xr->idrd = sqfs_meta_reader_create(file, cmp, super->id_table_start,
					   super->bytes_used);
	if (xr->idrd == NULL)
		goto fail_blocks;

	xr->kvrd = sqfs_meta_reader_create(file, cmp, super->id_table_start,
					   super->bytes_used);
	if (xr->kvrd == NULL)
		goto fail_idrd;

	xr->xattr_end = super->bytes_used;
	return 0;
fail_idrd:
	xr->idrd = sqfs_drop(xr->idrd);
fail_blocks:
	free(xr->id_block_starts);
	xr->id_block_starts = NULL;
	return err;
}

static int read_key_hdr(sqfs_xattr_reader_t *xr, sqfs_xattr_entry_t *key,
			const char **prefix)
{
	int ret;

	ret = sqfs_meta_reader_read(xr->kvrd, key, sizeof(*key));
	if (ret)
		return ret;

	key->type = le16toh(key->type);
	key->size = le16toh(key->size);

	*prefix = sqfs_get_xattr_prefix(key->type & SQFS_XATTR_PREFIX_MASK);
	if ((*prefix) == NULL)
		return SQFS_ERROR_UNSUPPORTED;

	return 0;
}

static int read_value_hdr(sqfs_xattr_reader_t *xr,
			  const sqfs_xattr_entry_t *key, sqfs_u64 *start,
			  size_t *offset, sqfs_xattr_value_t *value)
{
	sqfs_u64 ref, new_start;
	size_t new_offset;
	int ret;

	ret = sqfs_meta_reader_read(xr->kvrd, value, sizeof(*value));
	if (ret)
		return ret;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		ret = sqfs_meta_reader_read(xr->kvrd, &ref, sizeof(ref));
		if (ret)
			return ret;

		ref = le64toh(ref);
		new_start = xr->xattr_start + (ref >> 16);
		new_offset = ref & 0xFFFF;

		if (new_start >= xr->xattr_end ||
		    new_offset >= SQFS_META_BLOCK_SIZE) {
			return SQFS_ERROR_OUT_OF_BOUNDS;
		}

		sqfs_meta_reader_get_position(xr->kvrd, start, offset);

		ret = sqfs_meta_reader_seek(xr->kvrd, new_start, new_offset);
		if (ret)
			return ret;

		ret = sqfs_meta_reader_read(xr->kvrd, value, sizeof(*value));
		if (ret)
			return ret;
	}

	value->size = le32toh(value->size);
	return 0;
}

int sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr,
			       sqfs_xattr_entry_t **key_out)
{
	sqfs_xattr_entry_t key, *out;
	const char *prefix;
	size_t plen, total;
	int ret;

	ret = read_key_hdr(xr, &key, &prefix);
	if (ret)
		return ret;

	plen = strlen(prefix);

	if (SZ_ADD_OV(plen, key.size, &total) || SZ_ADD_OV(total, 1, &total) ||
	    SZ_ADD_OV(sizeof(*out), total, &total)) {
		return SQFS_ERROR_OVERFLOW;
	}

	out = calloc(1, total);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = key;
	memcpy(out->key, prefix, plen);

	ret = sqfs_meta_reader_read(xr->kvrd, out->key + plen, key.size);
	if (ret) {
		free(out);
		return ret;
	}

	*key_out = out;
	return 0;
}

int sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
				 const sqfs_xattr_entry_t *key,
				 sqfs_xattr_value_t **val_out)
{
	sqfs_xattr_value_t value, *out = NULL;
	size_t size, offset = 0;
	sqfs_u64 start = 0;
	int ret;

	ret = read_value_hdr(xr, key, &start, &offset, &value);
	if (ret)
		return ret;

	size = sizeof(*out) + 1;
	if (SZ_ADD_OV(size, value.size, &size))
		return SQFS_ERROR_OVERFLOW;

	out = calloc(1, size);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = value;

	ret = sqfs_meta_reader_read(xr->kvrd, out->value, value.size);
	if (ret)
		goto fail;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		ret = sqfs_meta_reader_seek(xr->kvrd, start, offset);
		if (ret)
			goto fail;
	}

	*val_out = out;
	return 0;
fail:
	free(out);
	return ret;
}

int sqfs_xattr_reader_read(sqfs_xattr_reader_t *xr, sqfs_xattr_t **out)
{
	sqfs_xattr_t *kv = NULL, *new = NULL;
	size_t plen, total = 0, offset = 0;
	sqfs_xattr_value_t value;
	sqfs_xattr_entry_t key;
	const char *prefix;
	sqfs_u64 start = 0;
	int ret;

	/* read and decode the key */
	ret = read_key_hdr(xr, &key, &prefix);
	if (ret)
		return ret;

	plen = strlen(prefix);
	total = sizeof(*kv) + plen + 1;

	if (SZ_ADD_OV(total, key.size, &total))
		return SQFS_ERROR_OVERFLOW;

	kv = calloc(1, total);
	if (kv == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(kv->data, prefix, plen);
	ret = sqfs_meta_reader_read(xr->kvrd, kv->data + plen, key.size);
	if (ret)
		goto fail;

	/* read and decode the value */
	ret = read_value_hdr(xr, &key, &start, &offset, &value);
	if (ret)
		goto fail;

	ret = SQFS_ERROR_OVERFLOW;
	if (SZ_ADD_OV(total, value.size, &total) || SZ_ADD_OV(total, 1, &total))
		goto fail;

	ret = SQFS_ERROR_ALLOC;
	new = realloc(kv, total);
	if (new == NULL)
		goto fail;
	kv = new;

	ret = sqfs_meta_reader_read(xr->kvrd, kv->data + plen + key.size + 1,
				    value.size);
	if (ret)
		goto fail;

	if (key.type & SQFS_XATTR_FLAG_OOL) {
		ret = sqfs_meta_reader_seek(xr->kvrd, start, offset);
		if (ret)
			goto fail;
	}

	/* prefix with the kv struct */
	kv->value_len = value.size;
	kv->key = (const char *)kv->data;
	kv->value = kv->data + plen + key.size + 1;
	kv->data[plen + key.size + 1 + value.size] = '\0';
	*out = kv;
	return 0;
fail:
	free(kv);
	return ret;
}

int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
			      const sqfs_xattr_id_t *desc)
{
	sqfs_u32 offset = desc->xattr & 0xFFFF;
	sqfs_u64 block = xr->xattr_start + (desc->xattr >> 16);

	return sqfs_meta_reader_seek(xr->kvrd, block, offset);
}

int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, sqfs_u32 idx,
			       sqfs_xattr_id_t *desc)
{
	size_t block, offset;
	int ret;

	memset(desc, 0, sizeof(*desc));

	if (idx == 0xFFFFFFFF)
		return 0;

	if (xr->kvrd == NULL || xr->idrd == NULL)
		return idx == 0 ? 0 : SQFS_ERROR_OUT_OF_BOUNDS;

	if (idx >= xr->num_ids)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	offset = (idx * sizeof(*desc)) % SQFS_META_BLOCK_SIZE;
	block = (idx * sizeof(*desc)) / SQFS_META_BLOCK_SIZE;

	ret = sqfs_meta_reader_seek(xr->idrd, xr->id_block_starts[block],
				    offset);
	if (ret)
		return ret;

	ret = sqfs_meta_reader_read(xr->idrd, desc, sizeof(*desc));
	if (ret)
		return ret;

	desc->xattr = le64toh(desc->xattr);
	desc->count = le32toh(desc->count);
	desc->size = le32toh(desc->size);
	return 0;
}

int sqfs_xattr_reader_read_all(sqfs_xattr_reader_t *xr, sqfs_u32 idx,
			       sqfs_xattr_t **out)
{
	sqfs_xattr_t *head = NULL, *tail = NULL;
	sqfs_xattr_id_t desc;
	int ret;

	*out = NULL;
	if (idx == 0xFFFFFFFF)
		return 0;

	ret = sqfs_xattr_reader_get_desc(xr, idx, &desc);
	if (ret)
		return ret;

	ret = sqfs_xattr_reader_seek_kv(xr, &desc);
	if (ret)
		return ret;

	for (size_t i = 0; i < desc.count; ++i) {
		sqfs_xattr_t *ent;

		ret = sqfs_xattr_reader_read(xr, &ent);
		if (ret)
			goto fail;

		if (tail == NULL) {
			head = ent;
			tail = ent;
		} else {
			tail->next = ent;
			tail = ent;
		}
	}

	*out = head;
	return 0;
fail:
	sqfs_xattr_list_free(head);
	return ret;
}

sqfs_xattr_reader_t *sqfs_xattr_reader_create(sqfs_u32 flags)
{
	sqfs_xattr_reader_t *xr;

	if (flags != 0)
		return NULL;

	xr = calloc(1, sizeof(*xr));
	if (xr == NULL)
		return NULL;

	sqfs_object_init(xr, xattr_reader_destroy, xattr_reader_copy);
	return xr;
}
