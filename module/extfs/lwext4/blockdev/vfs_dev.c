/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ext4_blockdev.h>
#include <ext4_config.h>
#include <ext4_errno.h>
#include <lwext4/blockdev/vfs_dev.h>
#include <stdbool.h>

const char *dev_name;

/**@brief   Image block size.*/
#define EXT4_FILEDEV_BSIZE 512

/**@brief   Image file descriptor.*/
static vfs_node_t dev_node;

#define DROP_LINUXCACHE_BUFFERS 0

/**********************BLOCKDEV INTERFACE**************************************/
static int vfs_dev_open(struct ext4_blockdev *bdev);
static int vfs_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);
static int vfs_dev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
                          uint32_t blk_cnt);
static int vfs_dev_close(struct ext4_blockdev *bdev);

/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(vfs_dev, EXT4_FILEDEV_BSIZE, 0, vfs_dev_open, vfs_dev_bread,
                              vfs_dev_bwrite, vfs_dev_close, 0, 0);

/******************************************************************************/
static int vfs_dev_open(struct ext4_blockdev *bdev) {
    dev_node = vfs_open(dev_name);

    vfs_dev.part_offset   = 0;
    vfs_dev.part_size     = 0xffffffffffffffff;
    vfs_dev.bdif->ph_bcnt = vfs_dev.part_size / vfs_dev.bdif->ph_bsize;

    return EOK;
}

/******************************************************************************/

static int vfs_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
    vfs_read(dev_node, buf, blk_id * vfs_dev.bdif->ph_bsize, blk_cnt * vfs_dev.bdif->ph_bsize);

    return EOK;
}

static void drop_cache(void) {}

/******************************************************************************/
static int vfs_dev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
                          uint32_t blk_cnt) {
    vfs_write(dev_node, buf, blk_id * vfs_dev.bdif->ph_bsize, blk_cnt * vfs_dev.bdif->ph_bsize);

    drop_cache();
    return EOK;
}
/******************************************************************************/
static int vfs_dev_close(struct ext4_blockdev *bdev) {
    vfs_close(dev_node);
    free(dev_name);
    return EOK;
}

/******************************************************************************/
struct ext4_blockdev *vfs_dev_get(void) {
    return &vfs_dev;
}
/******************************************************************************/

extern char *strdup(const char *);

void vfs_dev_name_set(const char *n) {
    dev_name = strdup(n);
}
/******************************************************************************/
