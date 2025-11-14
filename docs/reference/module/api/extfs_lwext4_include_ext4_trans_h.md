# extfs/lwext4/include/ext4_trans.h

## `#ifndef EXT4_TRANS_H #define EXT4_TRANS_H #ifdef __cplusplus extern "C" {`


@file  ext4_trans.h
Transaction handle functions


---

## `int ext4_trans_set_block_dirty(struct ext4_buf *buf);`

Mark a buffer dirty and add it to the current transaction.

- **`buf`**: buffer
- **Returns**: standard error code

---

## `int ext4_trans_block_get_noread(struct ext4_blockdev *bdev, struct ext4_block *b, uint64_t lba);`

Block get function (through cache, don't read).
jbd_trans_get_access would be called in order to
get write access to the buffer.

- **`bdev`**: block device descriptor
- **`b`**: block descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_trans_block_get(struct ext4_blockdev *bdev, struct ext4_block *b, uint64_t lba);`

Block get function (through cache).
jbd_trans_get_access would be called in order to
get write access to the buffer.

- **`bdev`**: block device descriptor
- **`b`**: block descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int ext4_trans_try_revoke_block(struct ext4_blockdev *bdev, uint64_t lba);`

Try to add block to be revoked to the current transaction.

- **`bdev`**: block device descriptor
- **`lba`**: logical block address
- **Returns**: standard error code

---

