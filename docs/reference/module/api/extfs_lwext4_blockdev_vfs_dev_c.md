# extfs/lwext4/blockdev/vfs_dev.c

## `static vfs_node_t dev_node;`

Image file descriptor.

---

## `static int vfs_dev_open(struct ext4_blockdev *bdev);`

*******************BLOCKDEV INTERFACE*************************************

---

## `EXT4_BLOCKDEV_STATIC_INSTANCE(vfs_dev, EXT4_FILEDEV_BSIZE, 0, vfs_dev_open, vfs_dev_bread, vfs_dev_bwrite, vfs_dev_close, 0, 0);`

**************************************************************************

---

## `static int vfs_dev_open(struct ext4_blockdev *bdev) {`

**************************************************************************

---

## `static int vfs_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {`

**************************************************************************

---

## `static int vfs_dev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {`

**************************************************************************

---

## `static int vfs_dev_close(struct ext4_blockdev *bdev) {`

**************************************************************************

---

## `struct ext4_blockdev *vfs_dev_get(void) {`

**************************************************************************

---

## `extern char *strdup(const char *);`

**************************************************************************

---

