# extfs/lwext4/include/ext4_blockdev.h

## `#ifndef EXT4_BLOCKDEV_H_ #define EXT4_BLOCKDEV_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_blockdev.h
Block device module.


---

## `int (*open)(struct ext4_blockdev *bdev);`

Open device function

- **`bdev`**: block device.

---

## `int (*bread)(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt);`

Block read function.

- **`bdev`**: block device
- **`buf`**: output buffer
- **`blk_id`**: block id
- **`blk_cnt`**: block count

---

## `int (*bwrite)(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt);`

Block write function.

- **`buf`**: input buffer
- **`blk_id`**: block id
- **`blk_cnt`**: block count

---

## `int (*close)(struct ext4_blockdev *bdev);`

Close device function.

- **`bdev`**: block device.

---

## `int (*lock)(struct ext4_blockdev *bdev);`

Lock block device. Required in multi partition mode
operations. Not mandatory field.

- **`bdev`**: block device.

---

## `int (*unlock)(struct ext4_blockdev *bdev);`

Unlock block device. Required in multi partition mode
operations. Not mandatory field.

- **`bdev`**: block device.

---

## `uint32_t ph_bsize;`

Block size (bytes): physical

---

## `uint64_t ph_bcnt;`

Block count: physical

---

## `uint8_t *ph_bbuf;`

Block size buffer: physical

---

## `uint32_t ph_refctr;`

Reference counter to block device interface

---

## `uint32_t bread_ctr;`

Physical read counter

---

## `uint32_t bwrite_ctr;`

Physical write counter

---

## `void *p_user;`

User data pointer

---

## `struct ext4_blockdev {`

Definition of the simple block device.

---

## `struct ext4_blockdev_iface *bdif;`

Block device interface

---

## `uint64_t part_offset;`

Offset in bdif. For multi partition mode.

---

## `uint64_t part_size;`

Part size in bdif. For multi partition mode.

---

## `struct ext4_bcache *bc;`

Block cache.

---

## `uint32_t lg_bsize;`

Block size (bytes) logical

---

## `uint64_t lg_bcnt;`

Block count: logical

---

## `uint32_t cache_write_back;`

Cache write back mode reference counter

---

## `struct ext4_fs *fs;`

The filesystem this block device belongs to. 

---

## `#define EXT4_BLOCKDEV_STATIC_INSTANCE(__name, __bsize, __bcnt, __open, __bread, \ __bwrite, __close, __lock, __unlock) \ static uint8_t __name##_ph_bbuf[(__bsize)];`

Static initialization of the block device.

---

## `int ext4_block_init(struct ext4_blockdev *bdev);`

Block device initialization.

- **`bdev`**: block device descriptor
- **Returns**: standard error code

---

## `int ext4_block_bind_bcache(struct ext4_blockdev *bdev, struct ext4_bcache *bc);`

Binds a bcache to block device.

- **`bdev`**: block device descriptor
- **`bc`**: block cache descriptor
- **Returns**: standard error code

---

## `int ext4_block_fini(struct ext4_blockdev *bdev);`

Close block device

- **`bdev`**: block device descriptor
- **Returns**: standard error code

---

## `int ext4_block_flush_buf(struct ext4_blockdev *bdev, struct ext4_buf *buf);`

Flush data in given buffer to disk.

- **`bdev`**: block device descriptor
- **`buf`**: buffer
- **Returns**: standard error code

---

## `int ext4_block_flush_lba(struct ext4_blockdev *bdev, uint64_t lba);`

Flush data in buffer of given lba to disk,
if that buffer exists in block cache.

- **`bdev`**: block device descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `void ext4_block_set_lb_size(struct ext4_blockdev *bdev, uint32_t lb_bsize);`

Set logical block size in block device.

- **`bdev`**: block device descriptor
- **`lb_bsize`**: logical block size (in bytes)

---

## `int ext4_block_get_noread(struct ext4_blockdev *bdev, struct ext4_block *b, uint64_t lba);`

Block get function (through cache, don't read).

- **`bdev`**: block device descriptor
- **`b`**: block descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_block_get(struct ext4_blockdev *bdev, struct ext4_block *b, uint64_t lba);`

Block get function (through cache).

- **`bdev`**: block device descriptor
- **`b`**: block descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_block_set(struct ext4_blockdev *bdev, struct ext4_block *b);`

Block set procedure (through cache).

- **`bdev`**: block device descriptor
- **`b`**: block descriptor
- **Returns**: standard error code

---

## `int ext4_blocks_get_direct(struct ext4_blockdev *bdev, void *buf, uint64_t lba, uint32_t cnt);`

Block read procedure (without cache)

- **`bdev`**: block device descriptor
- **`buf`**: output buffer
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_blocks_set_direct(struct ext4_blockdev *bdev, const void *buf, uint64_t lba, uint32_t cnt);`

Block write procedure (without cache)

- **`bdev`**: block device descriptor
- **`buf`**: output buffer
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_block_writebytes(struct ext4_blockdev *bdev, uint64_t off, const void *buf, uint32_t len);`

Write to block device (by direct address).

- **`bdev`**: block device descriptor
- **`off`**: byte offset in block device
- **`buf`**: input buffer
- **`len`**: length of the write buffer
- **Returns**: standard error code

---

## `int ext4_block_readbytes(struct ext4_blockdev *bdev, uint64_t off, void *buf, uint32_t len);`

Read freom block device (by direct address).

- **`bdev`**: block device descriptor
- **`off`**: byte offset in block device
- **`buf`**: input buffer
- **`len`**: length of the write buffer
- **Returns**: standard error code

---

## `int ext4_block_cache_flush(struct ext4_blockdev *bdev);`

Flush all dirty buffers to disk

- **`bdev`**: block device descriptor
- **Returns**: standard error code

---

## `int ext4_block_cache_write_back(struct ext4_blockdev *bdev, uint8_t on_off);`

Enable/disable write back cache mode

- **`bdev`**: block device descriptor
- **`on_off`**: 
!0 - ENABLE
0 - DISABLE (all delayed cache buffers will be flushed)

- **Returns**: standard error code

---

