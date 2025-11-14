# extfs/lwext4/src/ext4_fs.c

## `#include <ext4_config.h> #include <ext4_types.h> #include <ext4_misc.h> #include <ext4_errno.h> #include <ext4_debug.h> #include <ext4_trans.h> #include <ext4_fs.h> #include <ext4_blockdev.h> #include <ext4_super.h> #include <ext4_crc32.h> #include <ext4_block_group.h> #include <ext4_balloc.h> #include <ext4_bitmap.h> #include <ext4_inode.h> #include <ext4_ialloc.h> #include <ext4_extent.h> #include <fs_subsystem.h> int ext4_fs_init(struct ext4_fs *fs, struct ext4_blockdev *bdev, bool read_only) {`


@file  ext4_fs.c
More complex filesystem functions.


---

## `static bool ext4_block_in_group(struct ext4_sblock *s, ext4_fsblk_t baddr, uint32_t bgid) {`

Determine whether the block is inside the group.

- **`baddr`**: block address
- **`bgid`**: block group id
- **Returns**: Error code


---

## `static void ext4_fs_mark_bitmap_end(int start_bit, int end_bit, void *bitmap) {`

To avoid calling the atomic setbit hundreds or thousands of times, we only
need to use it within a single byte (to ensure we get endianness right).
We can use memset for the rest of the bitmap as there are no other users.


---

## `static int ext4_fs_init_block_bitmap(struct ext4_block_group_ref *bg_ref) {`

Initialize block bitmap in block group.

- **`bg_ref`**: Reference to block group
- **Returns**: Error code


---

## `static int ext4_fs_init_inode_bitmap(struct ext4_block_group_ref *bg_ref) {`

Initialize i-node bitmap in block group.

- **`bg_ref`**: Reference to block group
- **Returns**: Error code


---

## `static int ext4_fs_init_inode_table(struct ext4_block_group_ref *bg_ref) {`

Initialize i-node table in block group.

- **`bg_ref`**: Reference to block group
- **Returns**: Error code


---

## `static uint16_t ext4_fs_bg_checksum(struct ext4_sblock *sb, uint32_t bgid, struct ext4_bgroup *bg) {`

Compute checksum of block group descriptor.

- **`sb`**: Superblock
- **`bgid`**: Index of block group in the filesystem
- **`bg`**: Block group to compute checksum for
- **Returns**: Checksum value


---

## `static int ext4_fs_release_inode_block(struct ext4_inode_ref *inode_ref, ext4_lblk_t iblock) {`

Release data block from i-node

- **`inode_ref`**: I-node to release block from
- **`iblock`**: Logical block to be released
- **Returns**: Error code


---

## `ext4_fsblk_t ext4_fs_inode_to_goal_block(struct ext4_inode_ref *inode_ref) {`

Compute 'goal' for inode index

- **`inode_ref`**: Reference to inode, to allocate block for
- **Returns**: goal


---

## `int ext4_fs_indirect_find_goal(struct ext4_inode_ref *inode_ref, ext4_fsblk_t *goal) {`

Compute 'goal' for allocation algorithm (For blockmap).

- **`inode_ref`**: Reference to inode, to allocate block for
- **Returns**: error code


---

