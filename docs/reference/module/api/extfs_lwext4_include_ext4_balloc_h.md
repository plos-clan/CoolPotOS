# extfs/lwext4/include/ext4_balloc.h

## `#ifndef EXT4_BALLOC_H_ #define EXT4_BALLOC_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_balloc.h
Physical block allocator.


---

## `uint32_t ext4_balloc_get_bgid_of_block(struct ext4_sblock *s, ext4_fsblk_t baddr);`

Compute number of block group from block address.

- **`s`**: superblock pointer.
- **`baddr`**: Absolute address of block.
- **Returns**: Block group index


---

## `ext4_fsblk_t ext4_balloc_get_block_of_bgid(struct ext4_sblock *s, uint32_t bgid);`

Compute the starting block address of a block group

- **`s`**: superblock pointer.
- **`bgid`**: block group index
- **Returns**: Block address


---

## `void ext4_balloc_set_bitmap_csum(struct ext4_sblock *sb, struct ext4_bgroup *bg, void *bitmap);`

Calculate and set checksum of block bitmap.

- **`sb`**: superblock pointer.
- **`bg`**: block group
- **`bitmap`**: bitmap buffer


---

## `int ext4_balloc_free_block(struct ext4_inode_ref *inode_ref, ext4_fsblk_t baddr);`

Free block from inode.

- **`inode_ref`**: inode reference
- **`baddr`**: block address
- **Returns**: standard error code

---

## `int ext4_balloc_free_blocks(struct ext4_inode_ref *inode_ref, ext4_fsblk_t first, uint32_t count);`

Free blocks from inode.

- **`inode_ref`**: inode reference
- **`first`**: block address
- **`count`**: block count
- **Returns**: standard error code

---

## `int ext4_balloc_alloc_block(struct ext4_inode_ref *inode_ref, ext4_fsblk_t goal, ext4_fsblk_t *baddr);`

Allocate block procedure.

- **`inode_ref`**: inode reference
- **`baddr`**: allocated block address
- **Returns**: standard error code

---

## `int ext4_balloc_try_alloc_block(struct ext4_inode_ref *inode_ref, ext4_fsblk_t baddr, bool *free);`

Try allocate selected block.

- **`inode_ref`**: inode reference
- **`baddr`**: block address to allocate
- **`free`**: if baddr is not allocated
- **Returns**: standard error code

---

