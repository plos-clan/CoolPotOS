/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_UTIL_H
#define SQFS_UTIL_H

#include "config.h"
#include "sqfs/predef.h"
#include "compat.h"

/*
  Helper for allocating data structures with flexible array members.

  'base_size' is the size of the struct itself, 'item_size' the size of a
  single array element and 'nmemb' the number of elements.

  Iternally checks for arithmetic overflows when allocating the combined thing.
 */
SQFS_INTERNAL
void *alloc_flex(size_t base_size, size_t item_size, size_t nmemb);

/* Basically the same as calloc, but *ALWAYS* does overflow checking */
SQFS_INTERNAL
void *alloc_array(size_t item_size, size_t nmemb);

SQFS_INTERNAL sqfs_u32 xxh32(const void *input, const size_t len);

/*
  Returns true if the given region of memory is filled with zero-bytes only.
 */
SQFS_INTERNAL bool is_memory_zero(const void *blob, size_t size);

/*
  A wrapper around mkdir() that behaves like 'mkdir -p'. It tries to create
  every component of the given path and skips already existing entries.

  Returns 0 on success.
*/
SQFS_INTERNAL int mkdir_p(const char *path);

/*
  Remove all preceeding and trailing slashes, collapse all sequences of
  slashes, remove all path components that are '.' and returns failure
  state if one of the path components is '..'.

  Returns 0 on success.
*/
SQFS_INTERNAL int canonicalize_name(char *filename);

/*
  Returns true if a given filename is sane, false if it is not (e.g. contains
  slashes or it is equal to '.' or '..').

  If check_os_specific is true, this also checks if the filename contains
  a character, or is equal to a name, that is black listed on the current OS.
  E.g. on Windows, a file named "COM0" or "AUX" is a no-no.
 */
SQFS_INTERNAL bool is_filename_sane(const char *name, bool check_os_specific);

/*
  If the environment variable SOURCE_DATE_EPOCH is set to a parsable number
  that fits into an unsigned 32 bit value, return its value. Otherwise,
  default to 0.
 */
SQFS_INTERNAL sqfs_u32 get_source_date_epoch(void);

/*
  Check if two regions in a file are equal using a scratch buffer to load the
  data into for comparision. Returns 0 if equal, > 0 if not, < 0 if error.
 */
SQFS_INTERNAL int check_file_range_equal(sqfs_file_t *file, void *scratch,
					 size_t scratch_size, sqfs_u64 loc_a,
					 sqfs_u64 loc_b, sqfs_u64 size);

SQFS_INTERNAL int hex_decode(const char *in, size_t in_sz,
			     sqfs_u8 *out, size_t out_sz);

SQFS_INTERNAL int base64_decode(const char *in, size_t in_len,
				sqfs_u8 *out, size_t *out_len);

#endif /* SQFS_UTIL_H */
