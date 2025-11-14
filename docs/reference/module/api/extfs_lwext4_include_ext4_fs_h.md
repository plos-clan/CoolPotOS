# extfs/lwext4/include/ext4_fs.h

## `#ifndef EXT4_FS_H_ #define EXT4_FS_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_fs.c
More complex filesystem functions.


---

## `static inline uint32_t ext4_fs_addr_to_idx_bg(struct ext4_sblock *s, ext4_fsblk_t baddr) {`

Convert block address to relative index in block group.

- **`s`**: Superblock pointer
- **`baddr`**: Block number to convert
- **Returns**: Relative number of block


---

## `static inline ext4_fsblk_t ext4_fs_bg_idx_to_addr(struct ext4_sblock *s, uint32_t index, uint32_t bgid) {`

Convert relative block address in group to absolute address.

- **`s`**: Superblock pointer
- **`index`**: Relative block address
- **`bgid`**: Block group
- **Returns**: Absolute block address


---

## `static inline ext4_fsblk_t ext4_fs_first_bg_block_no(struct ext4_sblock *s, uint32_t bgid) {`

TODO: 

---

## `int ext4_fs_init(struct ext4_fs *fs, struct ext4_blockdev *bdev, bool read_only);`

Initialize filesystem and read all needed data.

- **`fs`**: Filesystem instance to be initialized
- **`bdev`**: Identifier if device with the filesystem
- **`read_only`**: Mark the filesystem as read-only.
- **Returns**: Error code


---

## `int ext4_fs_fini(struct ext4_fs *fs);`

Destroy filesystem instance (used by unmount operation).

- **`fs`**: Filesystem to be destroyed
- **Returns**: Error code


---

## `int ext4_fs_check_features(struct ext4_fs *fs, bool *read_only);`

Check filesystem's features, if supported by this driver
Function can return EOK and set read_only flag. It mean's that
there are some not-supported features, that can cause problems
during some write operations.

- **`fs`**: Filesystem to be checked
- **`read_only`**: Flag if filesystem should be mounted only for reading
- **Returns**: Error code


---

## `int ext4_fs_get_block_group_ref(struct ext4_fs *fs, uint32_t bgid, struct ext4_block_group_ref *ref);`

Get reference to block group specified by index.

- **`fs`**: Filesystem to find block group on
- **`bgid`**: Index of block group to load
- **`ref`**: Output pointer for reference
- **Returns**: Error code


---

## `int ext4_fs_put_block_group_ref(struct ext4_block_group_ref *ref);`

Put reference to block group.

- **`ref`**: Pointer for reference to be put back
- **Returns**: Error code


---

## `int ext4_fs_get_inode_ref(struct ext4_fs *fs, uint32_t index, struct ext4_inode_ref *ref);`

Get reference to i-node specified by index.

- **`fs`**: Filesystem to find i-node on
- **`index`**: Index of i-node to load
- **`ref`**: Output pointer for reference
- **Returns**: Error code


---

## `void ext4_fs_inode_blocks_init(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref);`

Reset blocks field of i-node.

- **`fs`**: Filesystem to reset blocks field of i-inode on
- **`inode_ref`**: ref Pointer for inode to be operated on


---

## `int ext4_fs_put_inode_ref(struct ext4_inode_ref *ref);`

Put reference to i-node.

- **`ref`**: Pointer for reference to be put back
- **Returns**: Error code


---

## `uint32_t ext4_fs_correspond_inode_mode(int filetype);`

Convert filetype to inode mode.

- **`filetype`**: File type
- **Returns**: inode mode


---

## `int ext4_fs_alloc_inode(struct ext4_fs *fs, struct ext4_inode_ref *inode_ref, int filetype);`

Allocate new i-node in the filesystem.

- **`fs`**: Filesystem to allocated i-node on
- **`inode_ref`**: Output pointer to return reference to allocated i-node
- **`filetype`**: File type of newly created i-node
- **Returns**: Error code


---

## `int ext4_fs_free_inode(struct ext4_inode_ref *inode_ref);`

Release i-node and mark it as free.

- **`inode_ref`**: I-node to be released
- **Returns**: Error code


---

## `int ext4_fs_truncate_inode(struct ext4_inode_ref *inode_ref, uint64_t new_size);`

Truncate i-node data blocks.

- **`inode_ref`**: I-node to be truncated
- **`new_size`**: New size of inode (must be < current size)
- **Returns**: Error code


---

## `ext4_fsblk_t ext4_fs_inode_to_goal_block(struct ext4_inode_ref *inode_ref);`

Compute 'goal' for inode index

- **`inode_ref`**: Reference to inode, to allocate block for
- **Returns**: goal


---

## `int ext4_fs_indirect_find_goal(struct ext4_inode_ref *inode_ref, ext4_fsblk_t *goal);`

Compute 'goal' for allocation algorithm (For blockmap).

- **`inode_ref`**: Reference to inode, to allocate block for
- **Returns**: error code


---

## `int ext4_fs_get_inode_dblk_idx(struct ext4_inode_ref *inode_ref, ext4_lblk_t iblock, ext4_fsblk_t *fblock, bool support_unwritten);`

Get physical block address by logical index of the block.

- **`inode_ref`**: I-node to read block address from
- **`iblock`**: Logical index of block
- **`fblock`**: Output pointer for return physical
block address

- **`support_unwritten`**: Indicate whether unwritten block range
is supported under the current context

- **Returns**: Error code


---

## `int ext4_fs_init_inode_dblk_idx(struct ext4_inode_ref *inode_ref, ext4_lblk_t iblock, ext4_fsblk_t *fblock);`

Initialize a part of unwritten range of the inode.

- **`inode_ref`**: I-node to proceed on.
- **`iblock`**: Logical index of block
- **`fblock`**: Output pointer for return physical block address
- **Returns**: Error code


---

## `int ext4_fs_append_inode_dblk(struct ext4_inode_ref *inode_ref, ext4_fsblk_t *fblock, ext4_lblk_t *iblock);`

Append following logical block to the i-node.

- **`inode_ref`**: I-node to append block to
- **`fblock`**: Output physical block address of newly allocated block
- **`iblock`**: Output logical number of newly allocated block
- **Returns**: Error code


---

## `void ext4_fs_inode_links_count_inc(struct ext4_inode_ref *inode_ref);`

Increment inode link count.

- **`inode_ref`**: none handle


---

## `void ext4_fs_inode_links_count_dec(struct ext4_inode_ref *inode_ref);`

Decrement inode link count.

- **`inode_ref`**: none handle


---

