# extfs/lwext4/blockdev/blockdev.c

## `static int blockdev_open(struct ext4_blockdev *bdev);`

*******************BLOCKDEV INTERFACE*************************************

---

## `EXT4_BLOCKDEV_STATIC_INSTANCE(blockdev, 512, 0, blockdev_open, blockdev_bread, blockdev_bwrite, blockdev_close, blockdev_lock, blockdev_unlock);`

**************************************************************************

---

## `static int blockdev_open(struct ext4_blockdev *bdev) {`

**************************************************************************

---

## `static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {`

**************************************************************************

---

## `static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {`

**************************************************************************

---

## `static int blockdev_close(struct ext4_blockdev *bdev) {`

**************************************************************************

---

## `struct ext4_blockdev *ext4_blockdev_get(void) {`

**************************************************************************

---

