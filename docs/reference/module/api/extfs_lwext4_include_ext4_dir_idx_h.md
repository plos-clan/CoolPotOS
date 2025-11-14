# extfs/lwext4/include/ext4_dir_idx.h

## `#ifndef EXT4_DIR_IDX_H_ #define EXT4_DIR_IDX_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_dir_idx.h
Directory indexing procedures.


---

## `int ext4_dir_dx_init(struct ext4_inode_ref *dir, struct ext4_inode_ref *parent);`

Initialize index structure of new directory.

- **`dir`**: Pointer to directory i-node
- **`parent`**: Pointer to parent directory i-node
- **Returns**: Error code


---

## `int ext4_dir_dx_find_entry(struct ext4_dir_search_result *result, struct ext4_inode_ref *inode_ref, size_t name_len, const char *name);`

Try to find directory entry using directory index.

- **`result`**: Output value - if entry will be found,
than will be passed through this parameter

- **`inode_ref`**: Directory i-node
- **`name_len`**: Length of name to be found
- **`name`**: Name to be found
- **Returns**: Error code


---

## `int ext4_dir_dx_add_entry(struct ext4_inode_ref *parent, struct ext4_inode_ref *child, const char *name, uint32_t name_len);`

Add new entry to indexed directory

- **`parent`**: Directory i-node
- **`child`**: I-node to be referenced from directory entry
- **`name`**: Name of new directory entry
- **Returns**: Error code


---

## `int ext4_dir_dx_reset_parent_inode(struct ext4_inode_ref *dir, uint32_t parent_inode);`

Add new entry to indexed directory

- **`dir`**: Directory i-node
- **`parent_inode`**: parent inode index
- **Returns**: Error code


---

