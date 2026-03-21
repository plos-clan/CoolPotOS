/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * read_inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/error.h"
#include "sqfs/super.h"
#include "sqfs/inode.h"
#include "sqfs/dir.h"
#include "util/util.h"

#include "cp_kernel.h"
#include "errno.h"
extern errno_t errno;

#define SWAB16(x) x = le16toh(x)
#define SWAB32(x) x = le32toh(x)
#define SWAB64(x) x = le64toh(x)

static int set_mode(sqfs_inode_t *inode) {
    inode->mode &= ~S_IFMT;

    switch (inode->type) {
    case SQFS_INODE_SOCKET:
    case SQFS_INODE_EXT_SOCKET:
        inode->mode |= S_IFSOCK;
        break;
    case SQFS_INODE_SLINK:
    case SQFS_INODE_EXT_SLINK:
        inode->mode |= S_IFLNK;
        break;
    case SQFS_INODE_FILE:
    case SQFS_INODE_EXT_FILE:
        inode->mode |= S_IFREG;
        break;
    case SQFS_INODE_BDEV:
    case SQFS_INODE_EXT_BDEV:
        inode->mode |= S_IFBLK;
        break;
    case SQFS_INODE_DIR:
    case SQFS_INODE_EXT_DIR:
        inode->mode |= S_IFDIR;
        break;
    case SQFS_INODE_CDEV:
    case SQFS_INODE_EXT_CDEV:
        inode->mode |= S_IFCHR;
        break;
    case SQFS_INODE_FIFO:
    case SQFS_INODE_EXT_FIFO:
        inode->mode |= S_IFIFO;
        break;
    default:
        return SQFS_ERROR_UNSUPPORTED;
    }

    return 0;
}

static sqfs_u64
get_block_count(sqfs_u64 size, sqfs_u64 block_size, sqfs_u32 frag_index, sqfs_u32 frag_offset) {
    sqfs_u64 count = size / block_size;

    if ((size % block_size) != 0 && (frag_index == 0xFFFFFFFF || frag_offset == 0xFFFFFFFF)) {
        ++count;
    }

    return count;
}

static int read_inode_file(
    sqfs_meta_reader_t *ir, sqfs_inode_t *base, size_t block_size, sqfs_inode_generic_t **result
) {
    sqfs_inode_generic_t *out;
    sqfs_inode_file_t file;
    sqfs_u64 i, count;
    int err;

    err = sqfs_meta_reader_read(ir, &file, sizeof(file));
    if (err)
        return err;

    SWAB32(file.blocks_start);
    SWAB32(file.fragment_index);
    SWAB32(file.fragment_offset);
    SWAB32(file.file_size);

    count = get_block_count(file.file_size, block_size, file.fragment_index, file.fragment_offset);

    out = alloc_flex(sizeof(*out), sizeof(sqfs_u32), count);
    if (out == NULL)
        return SQFS_ERROR_ALLOC;

    out->base                    = *base;
    out->data.file               = file;
    out->payload_bytes_available = count * sizeof(sqfs_u32);
    out->payload_bytes_used      = count * sizeof(sqfs_u32);

    err = sqfs_meta_reader_read(ir, out->extra, count * sizeof(sqfs_u32));
    if (err) {
        free(out);
        return err;
    }

    for (i = 0; i < count; ++i)
        SWAB32(out->extra[i]);

    *result = out;
    return 0;
}

static int read_inode_file_ext(
    sqfs_meta_reader_t *ir, sqfs_inode_t *base, size_t block_size, sqfs_inode_generic_t **result
) {
    sqfs_inode_file_ext_t file;
    sqfs_inode_generic_t *out;
    sqfs_u64 i, count;
    int err;

    err = sqfs_meta_reader_read(ir, &file, sizeof(file));
    if (err)
        return err;

    SWAB64(file.blocks_start);
    SWAB64(file.file_size);
    SWAB64(file.sparse);
    SWAB32(file.nlink);
    SWAB32(file.fragment_idx);
    SWAB32(file.fragment_offset);
    SWAB32(file.xattr_idx);

    count = get_block_count(file.file_size, block_size, file.fragment_idx, file.fragment_offset);

    out = alloc_flex(sizeof(*out), sizeof(sqfs_u32), count);
    if (out == NULL) {
        return errno == EOVERFLOW ? SQFS_ERROR_OVERFLOW : SQFS_ERROR_ALLOC;
    }

    out->base                    = *base;
    out->data.file_ext           = file;
    out->payload_bytes_available = count * sizeof(sqfs_u32);
    out->payload_bytes_used      = count * sizeof(sqfs_u32);

    err = sqfs_meta_reader_read(ir, out->extra, count * sizeof(sqfs_u32));
    if (err) {
        free(out);
        return err;
    }

    for (i = 0; i < count; ++i)
        SWAB32(out->extra[i]);

    *result = out;
    return 0;
}

static int
read_inode_slink(sqfs_meta_reader_t *ir, sqfs_inode_t *base, sqfs_inode_generic_t **result) {
    sqfs_inode_generic_t *out;
    sqfs_inode_slink_t slink;
    size_t size;
    int err;

    err = sqfs_meta_reader_read(ir, &slink, sizeof(slink));
    if (err)
        return err;

    SWAB32(slink.nlink);
    SWAB32(slink.target_size);

    if (SZ_ADD_OV(slink.target_size, 1, &size) || SZ_ADD_OV(sizeof(*out), size, &size)) {
        return SQFS_ERROR_OVERFLOW;
    }

    out = calloc(1, size);
    if (out == NULL)
        return SQFS_ERROR_ALLOC;

    out->payload_bytes_available = size - sizeof(*out);
    out->payload_bytes_used      = size - sizeof(*out) - 1;
    out->base                    = *base;
    out->data.slink              = slink;

    err = sqfs_meta_reader_read(ir, (void *)out->extra, slink.target_size);
    if (err) {
        free(out);
        return err;
    }

    *result = out;
    return 0;
}

static int
read_inode_slink_ext(sqfs_meta_reader_t *ir, sqfs_inode_t *base, sqfs_inode_generic_t **result) {
    sqfs_u32 xattr;
    int err;

    err = read_inode_slink(ir, base, result);
    if (err)
        return err;

    err = sqfs_meta_reader_read(ir, &xattr, sizeof(xattr));
    if (err) {
        free(*result);
        return err;
    }

    (*result)->data.slink_ext.xattr_idx = le32toh(xattr);
    return 0;
}

static int
read_inode_dir_ext(sqfs_meta_reader_t *ir, sqfs_inode_t *base, sqfs_inode_generic_t **result) {
    sqfs_inode_generic_t *out;
    sqfs_inode_dir_ext_t dir;
    int err;

    err = sqfs_meta_reader_read(ir, &dir, sizeof(dir));
    if (err)
        return err;

    SWAB32(dir.nlink);
    SWAB32(dir.size);
    SWAB32(dir.start_block);
    SWAB32(dir.parent_inode);
    SWAB16(dir.inodex_count);
    SWAB16(dir.offset);
    SWAB32(dir.xattr_idx);

    out = calloc(1, sizeof(*out));
    if (out == NULL)
        return SQFS_ERROR_ALLOC;

    out->base = *base;

    /*
     * Directory traversal only needs the fixed inode fields. The packed
     * directory index blob is an optional acceleration structure and is not
     * consumed by the current kernel-side squashfs path.
     */
    dir.inodex_count = 0;
    out->data.dir_ext = dir;
    *result                      = out;
    return 0;
}

int sqfs_meta_reader_read_inode(
    sqfs_meta_reader_t *ir,
    const sqfs_super_t *super,
    sqfs_u64 block_start,
    size_t offset,
    sqfs_inode_generic_t **result
) {
    sqfs_inode_generic_t *out;
    sqfs_inode_t inode;
    int err;

    /* read base inode */
    block_start += super->inode_table_start;

    err = sqfs_meta_reader_seek(ir, block_start, offset);
    if (err)
        return err;

    err = sqfs_meta_reader_read(ir, &inode, sizeof(inode));
    if (err)
        return err;

    SWAB16(inode.type);
    SWAB16(inode.mode);
    SWAB16(inode.uid_idx);
    SWAB16(inode.gid_idx);
    SWAB32(inode.mod_time);
    SWAB32(inode.inode_number);

    err = set_mode(&inode);
    if (err)
        return err;

    /* inode types where the size is variable */
    switch (inode.type) {
    case SQFS_INODE_FILE:
        return read_inode_file(ir, &inode, super->block_size, result);
    case SQFS_INODE_SLINK:
        return read_inode_slink(ir, &inode, result);
    case SQFS_INODE_EXT_FILE:
        return read_inode_file_ext(ir, &inode, super->block_size, result);
    case SQFS_INODE_EXT_SLINK:
        return read_inode_slink_ext(ir, &inode, result);
    case SQFS_INODE_EXT_DIR:
        return read_inode_dir_ext(ir, &inode, result);
    default:
        break;
    }

    /* everything else */
    out = calloc(1, sizeof(*out));
    if (out == NULL)
        return SQFS_ERROR_ALLOC;

    out->base = inode;

    switch (inode.type) {
    case SQFS_INODE_DIR:
        err = sqfs_meta_reader_read(ir, &out->data.dir, sizeof(out->data.dir));
        if (err)
            goto fail_free;

        SWAB32(out->data.dir.start_block);
        SWAB32(out->data.dir.nlink);
        SWAB16(out->data.dir.size);
        SWAB16(out->data.dir.offset);
        SWAB32(out->data.dir.parent_inode);
        break;
    case SQFS_INODE_BDEV:
    case SQFS_INODE_CDEV:
        err = sqfs_meta_reader_read(ir, &out->data.dev, sizeof(out->data.dev));
        if (err)
            goto fail_free;
        SWAB32(out->data.dev.nlink);
        SWAB32(out->data.dev.devno);
        break;
    case SQFS_INODE_FIFO:
    case SQFS_INODE_SOCKET:
        err = sqfs_meta_reader_read(ir, &out->data.ipc, sizeof(out->data.ipc));
        if (err)
            goto fail_free;
        SWAB32(out->data.ipc.nlink);
        break;
    case SQFS_INODE_EXT_BDEV:
    case SQFS_INODE_EXT_CDEV:
        err = sqfs_meta_reader_read(ir, &out->data.dev_ext, sizeof(out->data.dev_ext));
        if (err)
            goto fail_free;
        SWAB32(out->data.dev_ext.nlink);
        SWAB32(out->data.dev_ext.devno);
        SWAB32(out->data.dev_ext.xattr_idx);
        break;
    case SQFS_INODE_EXT_FIFO:
    case SQFS_INODE_EXT_SOCKET:
        err = sqfs_meta_reader_read(ir, &out->data.ipc_ext, sizeof(out->data.ipc_ext));
        if (err)
            goto fail_free;
        SWAB32(out->data.ipc_ext.nlink);
        SWAB32(out->data.ipc_ext.xattr_idx);
        break;
    default:
        err = SQFS_ERROR_UNSUPPORTED;
        goto fail_free;
    }

    *result = out;
    return 0;
fail_free:
    free(out);
    return err;
}
