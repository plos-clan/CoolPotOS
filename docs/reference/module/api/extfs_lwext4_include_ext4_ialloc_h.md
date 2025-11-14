# extfs/lwext4/include/ext4_ialloc.h

## `#ifndef EXT4_IALLOC_H_ #define EXT4_IALLOC_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_ialloc.c
Inode allocation procedures.


---

## `void ext4_ialloc_set_bitmap_csum(struct ext4_sblock *sb, struct ext4_bgroup *bg, void *bitmap);`

Calculate and set checksum of inode bitmap.

- **`sb`**: superblock pointer.
- **`bg`**: block group
- **`bitmap`**: bitmap buffer


---

## `int ext4_ialloc_free_inode(struct ext4_fs *fs, uint32_t index, bool is_dir);`

Free i-node number and modify filesystem data structers.

- **`fs`**: Filesystem, where the i-node is located
- **`index`**: Index of i-node to be release
- **`is_dir`**: Flag us for information whether i-node is directory or not


---

## `int ext4_ialloc_alloc_inode(struct ext4_fs *fs, uint32_t *index, bool is_dir);`

I-node allocation algorithm.
This is more simple algorithm, than Orlov allocator used
in the Linux kernel.

- **`fs`**: Filesystem to allocate i-node on
- **`index`**: Output value - allocated i-node number
- **`is_dir`**: Flag if allocated i-node will be file or directory
- **Returns**: Error code


---

