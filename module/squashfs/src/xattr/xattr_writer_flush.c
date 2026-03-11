/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer_flush.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xattr_writer.h"

static const char *hexmap = "0123456789ABCDEF";

static void *from_base32(const char *input, size_t *size_out)
{
	sqfs_u8 lo, hi, *out, *ptr;
	size_t len;

	len = strlen(input);
	*size_out = len / 2;

	out = malloc(*size_out);
	if (out == NULL)
		return NULL;

	ptr = out;

	while (*input != '\0') {
		lo = strchr(hexmap, *(input++)) - hexmap;
		hi = strchr(hexmap, *(input++)) - hexmap;

		*(ptr++) = lo | (hi << 4);
	}

	return out;
}

static sqfs_s32 write_key(sqfs_meta_writer_t *mw, const char *key,
			  bool value_is_ool)
{
	sqfs_xattr_entry_t kent;
	int type, err;
	size_t len;

	type = sqfs_get_xattr_prefix_id(key);
	assert(type >= 0);

	key = strchr(key, '.');
	assert(key != NULL);
	++key;
	len = strlen(key);

	if (value_is_ool)
		type |= SQFS_XATTR_FLAG_OOL;

	memset(&kent, 0, sizeof(kent));
	kent.type = htole16(type);
	kent.size = htole16(len);

	err = sqfs_meta_writer_append(mw, &kent, sizeof(kent));
	if (err)
		return err;

	err = sqfs_meta_writer_append(mw, key, len);
	if (err)
		return err;

	return sizeof(kent) + len;
}

static sqfs_s32 write_value(sqfs_meta_writer_t *mw, const char *value_str,
			    sqfs_u64 *value_ref_out)
{
	sqfs_xattr_value_t vent;
	sqfs_u32 offset;
	sqfs_u64 block;
	size_t size;
	void *value;
	int err;

	value = from_base32(value_str, &size);
	if (value == NULL)
		return SQFS_ERROR_ALLOC;

	memset(&vent, 0, sizeof(vent));
	vent.size = htole32(size);

	sqfs_meta_writer_get_position(mw, &block, &offset);
	*value_ref_out = (block << 16) | (offset & 0xFFFF);

	err = sqfs_meta_writer_append(mw, &vent, sizeof(vent));
	if (err)
		goto fail;

	err = sqfs_meta_writer_append(mw, value, size);
	if (err)
		goto fail;

	free(value);
	return sizeof(vent) + size;
fail:
	free(value);
	return err;
}

static sqfs_s32 write_value_ool(sqfs_meta_writer_t *mw, sqfs_u64 location)
{
	sqfs_xattr_value_t vent;
	sqfs_u64 ref;
	int err;

	memset(&vent, 0, sizeof(vent));
	vent.size = htole32(sizeof(location));
	ref = htole64(location);

	err = sqfs_meta_writer_append(mw, &vent, sizeof(vent));
	if (err)
		return err;

	err = sqfs_meta_writer_append(mw, &ref, sizeof(ref));
	if (err)
		return err;

	return sizeof(vent) + sizeof(ref);
}

static bool should_store_ool(const char *val_str, size_t refcount)
{
	if (refcount < 2)
		return false;

	/*
	  Storing in line needs this many bytes: refcount * len

	  Storing out-of-line needs this many: len + (refcount - 1) * 8

	  Out-of-line prefereable iff refcount > 1 and:
	      refcount * len > len + (refcount - 1) * 8
	   => refcount * len - len > (refcount - 1) * 8
	   => (refcount - 1) * len > (refcount - 1) * 8
	   => len > 8
	 */
	return (strlen(val_str) / 2) > sizeof(sqfs_u64);
}

static int write_block_pairs(const sqfs_xattr_writer_t *xwr,
			     sqfs_meta_writer_t *mw,
			     const kv_block_desc_t *blk,
			     sqfs_u64 *ool_locations)
{
	const char *key_str, *value_str;
	sqfs_s32 diff, total = 0;
	size_t i, refcount;
	sqfs_u64 ref;

	for (i = 0; i < blk->count; ++i) {
		sqfs_u64 ent = ((sqfs_u64 *)xwr->kv_pairs.data)[blk->start + i];
		sqfs_u32 key_idx = GET_KEY(ent);
		sqfs_u32 val_idx = GET_VALUE(ent);

		key_str = str_table_get_string(&xwr->keys, key_idx);
		value_str = str_table_get_string(&xwr->values, val_idx);

		if (ool_locations[val_idx] == 0xFFFFFFFFFFFFFFFFUL) {
			diff = write_key(mw, key_str, false);
			if (diff < 0)
				return diff;
			total += diff;

			diff = write_value(mw, value_str, &ref);
			if (diff < 0)
				return diff;
			total += diff;

			refcount = str_table_get_ref_count(&xwr->values,
							   val_idx);

			if (should_store_ool(value_str, refcount))
				ool_locations[val_idx] = ref;
		} else {
			diff = write_key(mw, key_str, true);
			if (diff < 0)
				return diff;
			total += diff;

			diff = write_value_ool(mw, ool_locations[val_idx]);
			if (diff < 0)
				return diff;
			total += diff;
		}
	}

	return total;
}

static int write_kv_pairs(const sqfs_xattr_writer_t *xwr,
			  sqfs_meta_writer_t *mw)
{
	sqfs_u64 block, *ool_locations;
	kv_block_desc_t *blk;
	sqfs_u32 offset;
	sqfs_s32 size;
	size_t i;

	ool_locations = alloc_array(sizeof(ool_locations[0]),
				    str_table_count(&xwr->values));
	if (ool_locations == NULL)
		return SQFS_ERROR_ALLOC;

	for (i = 0; i < str_table_count(&xwr->values); ++i)
		ool_locations[i] = 0xFFFFFFFFFFFFFFFFUL;

	for (blk = xwr->kv_block_first; blk != NULL; blk = blk->next) {
		sqfs_meta_writer_get_position(mw, &block, &offset);
		blk->start_ref = (block << 16) | (offset & 0xFFFF);

		size = write_block_pairs(xwr, mw, blk, ool_locations);
		if (size < 0) {
			free(ool_locations);
			return size;
		}

		blk->size_bytes = size;
	}

	free(ool_locations);
	return sqfs_meta_writer_flush(mw);
}

static int write_id_table(const sqfs_xattr_writer_t *xwr,
			  sqfs_meta_writer_t *mw,
			  sqfs_u64 *locations)
{
	sqfs_xattr_id_t id_ent;
	kv_block_desc_t *blk;
	sqfs_u32 offset;
	sqfs_u64 block;
	size_t i = 0;
	int err;

	locations[i++] = 0;

	for (blk = xwr->kv_block_first; blk != NULL; blk = blk->next) {
		memset(&id_ent, 0, sizeof(id_ent));
		id_ent.xattr = htole64(blk->start_ref);
		id_ent.count = htole32(blk->count);
		id_ent.size = htole32(blk->size_bytes);

		err = sqfs_meta_writer_append(mw, &id_ent, sizeof(id_ent));
		if (err)
			return err;

		sqfs_meta_writer_get_position(mw, &block, &offset);
		if (block != locations[i - 1])
			locations[i++] = block;
	}

	return sqfs_meta_writer_flush(mw);
}

static int write_location_table(const sqfs_xattr_writer_t *xwr,
				sqfs_u64 kv_start, sqfs_file_t *file,
				const sqfs_super_t *super, sqfs_u64 *locations,
				size_t loc_count)
{
	sqfs_xattr_id_table_t idtbl;
	int err;

	memset(&idtbl, 0, sizeof(idtbl));
	idtbl.xattr_table_start = htole64(kv_start);
	idtbl.xattr_ids = htole32(xwr->num_blocks);

	err = file->write_at(file, super->xattr_id_table_start,
			     &idtbl, sizeof(idtbl));
	if (err)
		return err;

	return file->write_at(file, super->xattr_id_table_start + sizeof(idtbl),
			      locations, sizeof(locations[0]) * loc_count);
}

static int alloc_location_table(const sqfs_xattr_writer_t *xwr,
				sqfs_u64 **tbl_out, size_t *szout)
{
	sqfs_u64 *locations;
	size_t size, count;

	if (SZ_MUL_OV(xwr->num_blocks, sizeof(sqfs_xattr_id_t), &size))
		return SQFS_ERROR_OVERFLOW;

	count = size / SQFS_META_BLOCK_SIZE;
	if (size % SQFS_META_BLOCK_SIZE)
		++count;

	locations = alloc_array(sizeof(sqfs_u64), count);
	if (locations == NULL)
		return SQFS_ERROR_ALLOC;

	*tbl_out = locations;
	*szout = count;
	return 0;
}

int sqfs_xattr_writer_flush(const sqfs_xattr_writer_t *xwr, sqfs_file_t *file,
			    sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	sqfs_u64 *locations = NULL, kv_start, id_start;
	sqfs_meta_writer_t *mw;
	size_t i, count;
	int err;

	if (xwr->kv_pairs.used == 0 || xwr->num_blocks == 0) {
		super->xattr_id_table_start = 0xFFFFFFFFFFFFFFFFUL;
		super->flags |= SQFS_FLAG_NO_XATTRS;
		return 0;
	}

	mw = sqfs_meta_writer_create(file, cmp, 0);
	if (mw == NULL)
		return SQFS_ERROR_ALLOC;

	kv_start = file->get_size(file);
	err = write_kv_pairs(xwr, mw);
	if (err)
		goto out;

	sqfs_meta_writer_reset(mw);

	id_start = file->get_size(file);
	err = alloc_location_table(xwr, &locations, &count);
	if (err)
		goto out;

	err = write_id_table(xwr, mw, locations);
	if (err)
		goto out;

	super->xattr_id_table_start = file->get_size(file);
	super->flags &= ~SQFS_FLAG_NO_XATTRS;

	for (i = 0; i < count; ++i)
		locations[i] = htole64(locations[i] + id_start);

	err = write_location_table(xwr, kv_start, file, super,
				   locations, count);
out:
	free(locations);
	sqfs_drop(mw);
	return err;
}
