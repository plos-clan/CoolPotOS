# extfs/lwext4/include/ext4_super.h

## `#ifndef EXT4_SUPER_H_ #define EXT4_SUPER_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_super.c
Superblock operations.


---

## `static inline uint64_t ext4_sb_get_blocks_cnt(struct ext4_sblock *s) {`

Blocks count get stored in superblock.

- **`s`**: superblock descriptor
- **Returns**: count of blocks

---

## `static inline void ext4_sb_set_blocks_cnt(struct ext4_sblock *s, uint64_t cnt) {`

Blocks count set  in superblock.

- **`s`**: superblock descriptor
- **`cnt`**: count of blocks

---

## `static inline uint64_t ext4_sb_get_free_blocks_cnt(struct ext4_sblock *s) {`

Free blocks count get stored in superblock.

- **`s`**: superblock descriptor
- **Returns**: free blocks

---

## `static inline void ext4_sb_set_free_blocks_cnt(struct ext4_sblock *s, uint64_t cnt) {`

Free blocks count set.

- **`s`**: superblock descriptor
- **`cnt`**: new value of free blocks

---

## `static inline uint32_t ext4_sb_get_block_size(struct ext4_sblock *s) {`

Block size get from superblock.

- **`s`**: superblock descriptor
- **Returns**: block size in bytes

---

## `static inline uint16_t ext4_sb_get_desc_size(struct ext4_sblock *s) {`

Block group descriptor size.

- **`s`**: superblock descriptor
- **Returns**: block group descriptor size in bytes

---

## `static inline bool ext4_sb_check_flag(struct ext4_sblock *s, uint32_t v) {`

Support check of flag.

- **`s`**: superblock descriptor
- **`v`**: flag to check
- **Returns**: true if flag is supported

---

## `static inline bool ext4_sb_feature_com(struct ext4_sblock *s, uint32_t v) {`

Support check of feature compatible.

- **`s`**: superblock descriptor
- **`v`**: feature to check
- **Returns**: true if feature is supported

---

## `static inline bool ext4_sb_feature_incom(struct ext4_sblock *s, uint32_t v) {`

Support check of feature incompatible.

- **`s`**: superblock descriptor
- **`v`**: feature to check
- **Returns**: true if feature is supported

---

## `static inline bool ext4_sb_feature_ro_com(struct ext4_sblock *s, uint32_t v) {`

Support check of read only flag.

- **`s`**: superblock descriptor
- **`v`**: flag to check
- **Returns**: true if flag is supported

---

## `static inline uint32_t ext4_sb_bg_to_flex(struct ext4_sblock *s, uint32_t block_group) {`

Block group to flex group.

- **`s`**: superblock descriptor
- **`block_group`**: block group
- **Returns**: flex group id

---

## `static inline uint32_t ext4_sb_flex_bg_size(struct ext4_sblock *s) {`

Flex block group size.

- **`s`**: superblock descriptor
- **Returns**: flex bg size

---

## `static inline uint32_t ext4_sb_first_meta_bg(struct ext4_sblock *s) {`

Return first meta block group id.

- **`s`**: superblock descriptor
- **Returns**: first meta_bg id 

---

## `uint32_t ext4_block_group_cnt(struct ext4_sblock *s);`

Returns a block group count.

- **`s`**: superblock descriptor
- **Returns**: count of block groups

---

## `uint32_t ext4_blocks_in_group_cnt(struct ext4_sblock *s, uint32_t bgid);`

Returns block count in block group
(last block group may have less blocks)

- **`s`**: superblock descriptor
- **`bgid`**: block group id
- **Returns**: blocks count

---

## `uint32_t ext4_inodes_in_group_cnt(struct ext4_sblock *s, uint32_t bgid);`

Returns inodes count in block group
(last block group may have less inodes)

- **`s`**: superblock descriptor
- **`bgid`**: block group id
- **Returns**: inodes count

---

## `int ext4_sb_write(struct ext4_blockdev *bdev, struct ext4_sblock *s);`

Superblock write.

- **`bdev`**: block device descriptor.
- **`s`**: superblock descriptor
- **Returns**: Standard error code 

---

## `int ext4_sb_read(struct ext4_blockdev *bdev, struct ext4_sblock *s);`

Superblock read.

- **`bdev`**: block device descriptor.
- **`s`**: superblock descriptor
- **Returns**: Standard error code 

---

## `bool ext4_sb_check(struct ext4_sblock *s);`

Superblock simple validation.

- **`s`**: superblock descriptor
- **Returns**: true if OK

---

## `bool ext4_sb_is_super_in_bg(struct ext4_sblock *s, uint32_t block_group);`

Superblock presence in block group.

- **`s`**: superblock descriptor
- **`block_group`**: block group id
- **Returns**: true if block group has superblock

---

## `bool ext4_sb_sparse(uint32_t group);`

TODO:

---

## `uint32_t ext4_bg_num_gdb(struct ext4_sblock *s, uint32_t group);`

TODO:

---

## `uint32_t ext4_num_base_meta_clusters(struct ext4_sblock *s, uint32_t block_group);`

TODO:

---

