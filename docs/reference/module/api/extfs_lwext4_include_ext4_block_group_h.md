# extfs/lwext4/include/ext4_block_group.h

## `#ifndef EXT4_BLOCK_GROUP_H_ #define EXT4_BLOCK_GROUP_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_block_group.h
Block group function set.


---

## `static inline uint64_t ext4_bg_get_block_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get address of block with data block bitmap.

- **`bg`**: pointer to block group
- **`s`**: pointer to superblock
- **Returns**: Address of block with block bitmap


---

## `static inline void ext4_bg_set_block_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s, uint64_t blk) {`

Set address of block with data block bitmap.

- **`bg`**: pointer to block group
- **`s`**: pointer to superblock
- **`blk`**: block to set


---

## `static inline uint64_t ext4_bg_get_inode_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get address of block with i-node bitmap.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Address of block with i-node bitmap


---

## `static inline void ext4_bg_set_inode_bitmap(struct ext4_bgroup *bg, struct ext4_sblock *s, uint64_t blk) {`

Set address of block with i-node bitmap.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`blk`**: block to set


---

## `static inline uint64_t ext4_bg_get_inode_table_first_block(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get address of the first block of the i-node table.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Address of first block of i-node table


---

## `static inline void ext4_bg_set_inode_table_first_block(struct ext4_bgroup *bg, struct ext4_sblock *s, uint64_t blk) {`

Set address of the first block of the i-node table.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`blk`**: block to set


---

## `static inline uint32_t ext4_bg_get_free_blocks_count(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get number of free blocks in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Number of free blocks in block group


---

## `static inline void ext4_bg_set_free_blocks_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt) {`

Set number of free blocks in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`cnt`**: Number of free blocks in block group


---

## `static inline uint32_t ext4_bg_get_free_inodes_count(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get number of free i-nodes in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Number of free i-nodes in block group


---

## `static inline void ext4_bg_set_free_inodes_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt) {`

Set number of free i-nodes in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`cnt`**: Number of free i-nodes in block group


---

## `static inline uint32_t ext4_bg_get_used_dirs_count(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get number of used directories in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Number of used directories in block group


---

## `static inline void ext4_bg_set_used_dirs_count(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt) {`

Set number of used directories in block group.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`cnt`**: Number of used directories in block group


---

## `static inline uint32_t ext4_bg_get_itable_unused(struct ext4_bgroup *bg, struct ext4_sblock *s) {`

Get number of unused i-nodes.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **Returns**: Number of unused i-nodes


---

## `static inline void ext4_bg_set_itable_unused(struct ext4_bgroup *bg, struct ext4_sblock *s, uint32_t cnt) {`

Set number of unused i-nodes.

- **`bg`**: Pointer to block group
- **`s`**: Pointer to superblock
- **`cnt`**: Number of unused i-nodes


---

## `static inline void ext4_bg_set_checksum(struct ext4_bgroup *bg, uint16_t crc) {`

Set checksum of block group.

- **`bg`**: Pointer to block group
- **`crc`**: Cheksum of block group


---

## `static inline bool ext4_bg_has_flag(struct ext4_bgroup *bg, uint32_t f) {`

Check if block group has a flag.

- **`bg`**: Pointer to block group
- **`f`**: Flag to be checked
- **Returns**: True if flag is set to 1


---

## `static inline void ext4_bg_set_flag(struct ext4_bgroup *bg, uint32_t f) {`

Set flag of block group.

- **`bg`**: Pointer to block group
- **`f`**: Flag to be set


---

## `static inline void ext4_bg_clear_flag(struct ext4_bgroup *bg, uint32_t f) {`

Clear flag of block group.

- **`bg`**: Pointer to block group
- **`f`**: Flag to be cleared


---

## `uint16_t ext4_bg_crc16(uint16_t crc, const uint8_t *buffer, size_t len);`

Calculate CRC16 of the block group.

- **`crc`**: Init value
- **`buffer`**: Input buffer
- **`len`**: Sizeof input buffer
- **Returns**: Computed CRC16

---

