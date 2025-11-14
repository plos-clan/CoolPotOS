# extfs/lwext4/include/ext4_extent.h

## `#ifndef EXT4_EXTENT_H_ #define EXT4_EXTENT_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_extent.h
More complex filesystem functions.


---

## `int ext4_extent_remove_space(struct ext4_inode_ref *inode_ref, ext4_lblk_t from, ext4_lblk_t to);`

Release all data blocks starting from specified logical block.

- **`inode_ref`**: I-node to release blocks from
- **`iblock_from`**: First logical block to release
- **Returns**: Error code 

---

