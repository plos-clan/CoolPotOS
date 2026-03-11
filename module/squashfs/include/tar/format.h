/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * format.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TAR_FORMAT_H
#define TAR_FORMAT_H

#include "sqfs/predef.h"

typedef struct {
	char offset[12];
	char numbytes[12];
} gnu_old_sparse_t;

typedef struct {
	gnu_old_sparse_t sparse[21];
	char isextended;
	char padding[7];
} gnu_old_sparse_record_t;

typedef struct {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	union {
		struct {
			char prefix[155];
			char padding[12];
		} posix;

		struct {
			char atime[12];
			char ctime[12];
			char offset[12];
			char deprecated[4];
			char unused;
			gnu_old_sparse_t sparse[4];
			char isextended;
			char realsize[12];
			char padding[17];
		} gnu;
	} tail;
} tar_header_t;

#define TAR_TYPE_FILE '0'
#define TAR_TYPE_LINK '1'
#define TAR_TYPE_SLINK '2'
#define TAR_TYPE_CHARDEV '3'
#define TAR_TYPE_BLOCKDEV '4'
#define TAR_TYPE_DIR '5'
#define TAR_TYPE_FIFO '6'

#define TAR_TYPE_GNU_SLINK 'K'
#define TAR_TYPE_GNU_PATH 'L'
#define TAR_TYPE_GNU_SPARSE 'S'

#define TAR_TYPE_PAX 'x'
#define TAR_TYPE_PAX_GLOBAL 'g'

#define TAR_MAGIC "ustar"
#define TAR_VERSION "00"

#define TAR_MAGIC_OLD "ustar "
#define TAR_VERSION_OLD " "

#define TAR_RECORD_SIZE (512)

/* artificially imposed implementation limits */
#define TAR_MAX_SYMLINK_LEN (65536)
#define TAR_MAX_PATH_LEN (65536)
#define TAR_MAX_PAX_LEN (65536)
#define TAR_MAX_SPARSE_ENT (65536)

#ifdef __cplusplus
extern "C" {
#endif

int read_number(const char *str, int digits, sqfs_u64 *out);

unsigned int tar_compute_checksum(const tar_header_t *hdr);

#ifdef __cplusplus
}
#endif

#endif /* TAR_FORMAT_H */
