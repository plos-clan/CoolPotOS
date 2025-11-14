# extfs/lwext4/src/ext4_balloc.c

## `uint32_t ext4_balloc_get_bgid_of_block(struct ext4_sblock *s, uint64_t baddr) {`

Compute number of block group from block address.

- **`s`**: superblock pointer.
- **`baddr`**: Absolute address of block.
- **Returns**: Block group index


---

## `uint64_t ext4_balloc_get_block_of_bgid(struct ext4_sblock *s, uint32_t bgid) {`

Compute the starting block address of a block group

- **`s`**: superblock pointer.
- **`bgid`**: block group index
- **Returns**: Block address


---

