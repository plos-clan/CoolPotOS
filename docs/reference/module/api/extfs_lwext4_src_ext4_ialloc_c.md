# extfs/lwext4/src/ext4_ialloc.c

## `static uint32_t ext4_ialloc_inode_to_bgidx(struct ext4_sblock *sb, uint32_t inode) {`

Convert i-node number to relative index in block group.

- **`sb`**: Superblock
- **`inode`**: I-node number to be converted
- **Returns**: Index of the i-node in the block group


---

## `static uint32_t ext4_ialloc_bgidx_to_inode(struct ext4_sblock *sb, uint32_t index, uint32_t bgid) {`

Convert relative index of i-node to absolute i-node number.

- **`sb`**: Superblock
- **`index`**: Index to be converted
- **Returns**: Absolute number of the i-node



---

## `static uint32_t ext4_ialloc_get_bgid_of_inode(struct ext4_sblock *sb, uint32_t inode) {`

Compute block group number from the i-node number.

- **`sb`**: Superblock
- **`inode`**: I-node number to be found the block group for
- **Returns**: Block group number computed from i-node number


---

