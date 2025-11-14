# extfs/lwext4/src/ext4_bcache.c

## `#include <ext4_bcache.h> #include <ext4_blockdev.h> #include <ext4_config.h> #include <ext4_debug.h> #include <ext4_errno.h> #include <ext4_types.h> #include <fs_subsystem.h> static int ext4_bcache_lba_compare(struct ext4_buf *a, struct ext4_buf *b) {`


@file  ext4_bcache.c
Block cache allocator.


---

## `static struct ext4_buf *ext4_buf_alloc(struct ext4_bcache *bc, uint64_t lba) {`

:

This is ext4_bcache, the module handling basic buffer-cache stuff.

Buffers in a bcache are sorted by their LBA and stored in a
RB-Tree(lba_root).

Bcache also maintains another RB-Tree(lru_root) right now, where
buffers are sorted by their LRU id.

A singly-linked list is used to track those dirty buffers which are
ready to be flushed. (Those buffers which are dirty but also referenced
are not considered ready to be flushed.)

When a buffer is not referenced, it will be stored in both lba_root
and lru_root, while it will only be stored in lba_root when it is
referenced.


---

