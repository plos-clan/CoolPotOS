# extfs/lwext4/include/ext4_dir.h

## `#ifndef EXT4_DIR_H_ #define EXT4_DIR_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_dir.h
Directory handle procedures.


---

## `static inline uint32_t ext4_dir_en_get_inode(struct ext4_dir_en *de) {`

Get i-node number from directory entry.

- **`de`**: Directory entry
- **Returns**: I-node number


---

## `static inline void ext4_dir_en_set_inode(struct ext4_dir_en *de, uint32_t inode) {`

Set i-node number to directory entry.

- **`de`**: Directory entry
- **`inode`**: I-node number


---

## `static inline void ext4_dx_dot_en_set_inode(struct ext4_dir_idx_dot_en *de, uint32_t inode) {`

Set i-node number to directory entry. (For HTree root)

- **`de`**: Directory entry
- **`inode`**: I-node number


---

## `static inline uint16_t ext4_dir_en_get_entry_len(struct ext4_dir_en *de) {`

Get directory entry length.

- **`de`**: Directory entry
- **Returns**: Entry length


---

## `static inline void ext4_dir_en_set_entry_len(struct ext4_dir_en *de, uint16_t l) {`

Set directory entry length.

- **`de`**: Directory entry
- **`l`**: Entry length


---

## `static inline uint16_t ext4_dir_en_get_name_len(struct ext4_sblock *sb, struct ext4_dir_en *de) {`

Get directory entry name length.

- **`sb`**: Superblock
- **`de`**: Directory entry
- **Returns**: Entry name length


---

## `static inline void ext4_dir_en_set_name_len(struct ext4_sblock *sb, struct ext4_dir_en *de, uint16_t len) {`

Set directory entry name length.

- **`sb`**: Superblock
- **`de`**: Directory entry
- **`len`**: Entry name length


---

## `static inline uint8_t ext4_dir_en_get_inode_type(struct ext4_sblock *sb, struct ext4_dir_en *de) {`

Get i-node type of directory entry.

- **`sb`**: Superblock
- **`de`**: Directory entry
- **Returns**: I-node type (file, dir, etc.)


---

## `static inline void ext4_dir_en_set_inode_type(struct ext4_sblock *sb, struct ext4_dir_en *de, uint8_t t) {`

Set i-node type of directory entry.

- **`sb`**: Superblock
- **`de`**: Directory entry
- **`t`**: I-node type (file, dir, etc.)


---

## `bool ext4_dir_csum_verify(struct ext4_inode_ref *inode_ref, struct ext4_dir_en *dirent);`

Verify checksum of a linear directory leaf block

- **`inode_ref`**: Directory i-node
- **`dirent`**: Linear directory leaf block
- **Returns**: true means the block passed checksum verification


---

## `int ext4_dir_iterator_init(struct ext4_dir_iter *it, struct ext4_inode_ref *inode_ref, uint64_t pos);`

Initialize directory iterator.
Set position to the first valid entry from the required position.

- **`it`**: Pointer to iterator to be initialized
- **`inode_ref`**: Directory i-node
- **`pos`**: Position to start reading entries from
- **Returns**: Error code


---

## `int ext4_dir_iterator_next(struct ext4_dir_iter *it);`

Jump to the next valid entry

- **`it`**: Initialized iterator
- **Returns**: Error code


---

## `int ext4_dir_iterator_fini(struct ext4_dir_iter *it);`

Uninitialize directory iterator.
Release all allocated structures.

- **`it`**: Iterator to be finished
- **Returns**: Error code


---

## `void ext4_dir_write_entry(struct ext4_sblock *sb, struct ext4_dir_en *en, uint16_t entry_len, struct ext4_inode_ref *child, const char *name, size_t name_len);`

Write directory entry to concrete data block.

- **`sb`**: Superblock
- **`en`**: Pointer to entry to be written
- **`entry_len`**: Length of new entry
- **`child`**: Child i-node to be written to new entry
- **`name`**: Name of the new entry
- **`name_len`**: Length of entry name


---

## `int ext4_dir_add_entry(struct ext4_inode_ref *parent, const char *name, uint32_t name_len, struct ext4_inode_ref *child);`

Add new entry to the directory.

- **`parent`**: Directory i-node
- **`name`**: Name of new entry
- **`child`**: I-node to be referenced from new entry
- **Returns**: Error code


---

## `int ext4_dir_find_entry(struct ext4_dir_search_result *result, struct ext4_inode_ref *parent, const char *name, uint32_t name_len);`

Find directory entry with passed name.

- **`result`**: Result structure to be returned if entry found
- **`parent`**: Directory i-node
- **`name`**: Name of entry to be found
- **`name_len`**: Name length
- **Returns**: Error code


---

## `int ext4_dir_remove_entry(struct ext4_inode_ref *parent, const char *name, uint32_t name_len);`

Remove directory entry.

- **`parent`**: Directory i-node
- **`name`**: Name of the entry to be removed
- **`name_len`**: Name length
- **Returns**: Error code


---

## `int ext4_dir_try_insert_entry(struct ext4_sblock *sb, struct ext4_inode_ref *inode_ref, struct ext4_block *dst_blk, struct ext4_inode_ref *child, const char *name, uint32_t name_len);`

Try to insert entry to concrete data block.

- **`sb`**: Superblock
- **`inode_ref`**: Directory i-node
- **`dst_blk`**: Block to try to insert entry to
- **`child`**: Child i-node to be inserted by new entry
- **`name`**: Name of the new entry
- **`name_len`**: Length of the new entry name
- **Returns**: Error code


---

## `int ext4_dir_find_in_block(struct ext4_block *block, struct ext4_sblock *sb, size_t name_len, const char *name, struct ext4_dir_en **res_entry);`

Try to find entry in block by name.

- **`block`**: Block containing entries
- **`sb`**: Superblock
- **`name_len`**: Length of entry name
- **`name`**: Name of entry to be found
- **`res_entry`**: Output pointer to found entry, NULL if not found
- **Returns**: Error code


---

## `int ext4_dir_destroy_result(struct ext4_inode_ref *parent, struct ext4_dir_search_result *result);`

Simple function to release allocated data from result.

- **`parent`**: Parent inode
- **`result`**: Search result to destroy
- **Returns**: Error code



---

