# extfs/lwext4/include/ext4_inode.h

## `#ifndef EXT4_INODE_H_ #define EXT4_INODE_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_inode.h
Inode handle functions


---

## `uint32_t ext4_inode_get_mode(struct ext4_sblock *sb, struct ext4_inode *inode);`

Get mode of the i-node.

- **`sb`**: Superblock
- **`inode`**: I-node to load mode from
- **Returns**: Mode of the i-node


---

## `void ext4_inode_set_mode(struct ext4_sblock *sb, struct ext4_inode *inode, uint32_t mode);`

Set mode of the i-node.

- **`sb`**: Superblock
- **`inode`**: I-node to set mode to
- **`mode`**: Mode to set to i-node


---

## `uint32_t ext4_inode_get_uid(struct ext4_inode *inode);`

Get ID of the i-node owner (user id).

- **`inode`**: I-node to load uid from
- **Returns**: User ID of the i-node owner


---

## `void ext4_inode_set_uid(struct ext4_inode *inode, uint32_t uid);`

Set ID of the i-node owner.

- **`inode`**: I-node to set uid to
- **`uid`**: ID of the i-node owner


---

## `uint64_t ext4_inode_get_size(struct ext4_sblock *sb, struct ext4_inode *inode);`

Get real i-node size.

- **`sb`**: Superblock
- **`inode`**: I-node to load size from
- **Returns**: Real size of i-node


---

## `void ext4_inode_set_size(struct ext4_inode *inode, uint64_t size);`

Set real i-node size.

- **`inode`**: I-node to set size to
- **`size`**: Size of the i-node


---

## `uint32_t ext4_inode_get_access_time(struct ext4_inode *inode);`

Get time, when i-node was last accessed.

- **`inode`**: I-node
- **Returns**: Time of the last access (POSIX)


---

## `void ext4_inode_set_access_time(struct ext4_inode *inode, uint32_t time);`

Set time, when i-node was last accessed.

- **`inode`**: I-node
- **`time`**: Time of the last access (POSIX)


---

## `uint32_t ext4_inode_get_change_inode_time(struct ext4_inode *inode);`

Get time, when i-node was last changed.

- **`inode`**: I-node
- **Returns**: Time of the last change (POSIX)


---

## `void ext4_inode_set_change_inode_time(struct ext4_inode *inode, uint32_t time);`

Set time, when i-node was last changed.

- **`inode`**: I-node
- **`time`**: Time of the last change (POSIX)


---

## `uint32_t ext4_inode_get_modif_time(struct ext4_inode *inode);`

Get time, when i-node content was last modified.

- **`inode`**: I-node
- **Returns**: Time of the last content modification (POSIX)


---

## `void ext4_inode_set_modif_time(struct ext4_inode *inode, uint32_t time);`

Set time, when i-node content was last modified.

- **`inode`**: I-node
- **`time`**: Time of the last content modification (POSIX)


---

## `uint32_t ext4_inode_get_del_time(struct ext4_inode *inode);`

Get time, when i-node was deleted.

- **`inode`**: I-node
- **Returns**: Time of the delete action (POSIX)


---

## `void ext4_inode_set_del_time(struct ext4_inode *inode, uint32_t time);`

Set time, when i-node was deleted.

- **`inode`**: I-node
- **`time`**: Time of the delete action (POSIX)


---

## `uint32_t ext4_inode_get_gid(struct ext4_inode *inode);`

Get ID of the i-node owner's group.

- **`inode`**: I-node to load gid from
- **Returns**: Group ID of the i-node owner


---

## `void ext4_inode_set_gid(struct ext4_inode *inode, uint32_t gid);`

Set ID to the i-node owner's group.

- **`inode`**: I-node to set gid to
- **`gid`**: Group ID of the i-node owner


---

## `uint16_t ext4_inode_get_links_cnt(struct ext4_inode *inode);`

Get number of links to i-node.

- **`inode`**: I-node to load number of links from
- **Returns**: Number of links to i-node


---

## `void ext4_inode_set_links_cnt(struct ext4_inode *inode, uint16_t cnt);`

Set number of links to i-node.

- **`inode`**: I-node to set number of links to
- **`cnt`**: Number of links to i-node


---

## `uint64_t ext4_inode_get_blocks_count(struct ext4_sblock *sb, struct ext4_inode *inode);`

Get number of 512-bytes blocks used for i-node.

- **`sb`**: Superblock
- **`inode`**: I-node
- **Returns**: Number of 512-bytes blocks


---

## `int ext4_inode_set_blocks_count(struct ext4_sblock *sb, struct ext4_inode *inode, uint64_t cnt);`

Set number of 512-bytes blocks used for i-node.

- **`sb`**: Superblock
- **`inode`**: I-node
- **`cnt`**: Number of 512-bytes blocks
- **Returns**: Error code


---

## `uint32_t ext4_inode_get_flags(struct ext4_inode *inode);`

Get flags (features) of i-node.

- **`inode`**: I-node to get flags from
- **Returns**: Flags (bitmap)


---

## `void ext4_inode_set_flags(struct ext4_inode *inode, uint32_t flags);`

Set flags (features) of i-node.

- **`inode`**: I-node to set flags to
- **`flags`**: Flags to set to i-node


---

## `uint32_t ext4_inode_get_generation(struct ext4_inode *inode);`

Get file generation (used by NFS).

- **`inode`**: I-node
- **Returns**: File generation


---

## `void ext4_inode_set_generation(struct ext4_inode *inode, uint32_t gen);`

Set file generation (used by NFS).

- **`inode`**: I-node
- **`gen`**: File generation


---

## `uint16_t ext4_inode_get_extra_isize(struct ext4_sblock *sb, struct ext4_inode *inode);`

Get extra I-node size field.

- **`sb`**: Superblock
- **`inode`**: I-node
- **Returns**: extra I-node size


---

## `void ext4_inode_set_extra_isize(struct ext4_sblock *sb, struct ext4_inode *inode, uint16_t size);`

Set extra I-node size field.

- **`sb`**: Superblock
- **`inode`**: I-node
- **`size`**: extra I-node size


---

## `uint64_t ext4_inode_get_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb);`

Get address of block, where are extended attributes located.

- **`inode`**: I-node
- **`sb`**: Superblock
- **Returns**: Block address


---

## `void ext4_inode_set_file_acl(struct ext4_inode *inode, struct ext4_sblock *sb, uint64_t acl);`

Set address of block, where are extended attributes located.

- **`inode`**: I-node
- **`sb`**: Superblock
- **`acl`**: Block address


---

## `uint32_t ext4_inode_get_direct_block(struct ext4_inode *inode, uint32_t idx);`

Get block address of specified direct block.

- **`inode`**: I-node to load block from
- **`idx`**: Index of logical block
- **Returns**: Physical block address


---

## `void ext4_inode_set_direct_block(struct ext4_inode *inode, uint32_t idx, uint32_t block);`

Set block address of specified direct block.

- **`inode`**: I-node to set block address to
- **`idx`**: Index of logical block
- **`block`**: Physical block address


---

## `uint32_t ext4_inode_get_indirect_block(struct ext4_inode *inode, uint32_t idx);`

Get block address of specified indirect block.

- **`inode`**: I-node to get block address from
- **`idx`**: Index of indirect block
- **Returns**: Physical block address


---

## `void ext4_inode_set_indirect_block(struct ext4_inode *inode, uint32_t idx, uint32_t block);`

Set block address of specified indirect block.

- **`inode`**: I-node to set block address to
- **`idx`**: Index of indirect block
- **`block`**: Physical block address


---

## `uint32_t ext4_inode_get_dev(struct ext4_inode *inode);`

Get device number

- **`inode`**: I-node to get device number from
- **Returns**: Device number


---

## `void ext4_inode_set_dev(struct ext4_inode *inode, uint32_t dev);`

Set device number

- **`inode`**: I-node to set device number to
- **`dev`**: Device number


---

## `uint32_t ext4_inode_type(struct ext4_sblock *sb, struct ext4_inode *inode);`

return the type of i-node

- **`sb`**: Superblock
- **`inode`**: I-node to return the type of
- **Returns**: Result of check operation


---

## `bool ext4_inode_is_type(struct ext4_sblock *sb, struct ext4_inode *inode, uint32_t type);`

Check if i-node has specified type.

- **`sb`**: Superblock
- **`inode`**: I-node to check type of
- **`type`**: Type to check
- **Returns**: Result of check operation


---

## `bool ext4_inode_has_flag(struct ext4_inode *inode, uint32_t f);`

Check if i-node has specified flag.

- **`inode`**: I-node to check flags of
- **`f`**: Flag to check
- **Returns**: Result of check operation


---

## `void ext4_inode_clear_flag(struct ext4_inode *inode, uint32_t f);`

Remove specified flag from i-node.

- **`inode`**: I-node to clear flag on
- **`f`**: Flag to be cleared


---

## `void ext4_inode_set_flag(struct ext4_inode *inode, uint32_t f);`

Set specified flag to i-node.

- **`inode`**: I-node to set flag on
- **`f`**: Flag to be set


---

## `uint32_t ext4_inode_get_csum(struct ext4_sblock *sb, struct ext4_inode *inode);`

Get inode checksum(crc32)

- **`sb`**: Superblock
- **`inode`**: I-node to get checksum value from


---

## `void ext4_inode_set_csum(struct ext4_sblock *sb, struct ext4_inode *inode, uint32_t checksum);`

Get inode checksum(crc32)

- **`sb`**: Superblock
- **`inode`**: I-node to get checksum value from


---

## `bool ext4_inode_can_truncate(struct ext4_sblock *sb, struct ext4_inode *inode);`

Check if i-node can be truncated.

- **`sb`**: Superblock
- **`inode`**: I-node to check
- **Returns**: Result of the check operation


---

## `struct ext4_extent_header * ext4_inode_get_extent_header(struct ext4_inode *inode);`

Get extent header from the root of the extent tree.

- **`inode`**: I-node to get extent header from
- **Returns**: Pointer to extent header of the root node


---

