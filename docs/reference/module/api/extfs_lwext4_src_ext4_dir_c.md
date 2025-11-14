# extfs/lwext4/src/ext4_dir.c

## `/* Walk through a dirent block to find a checksum "dirent" at the tail */ static struct ext4_dir_entry_tail *ext4_dir_get_tail(struct ext4_inode_ref *inode_ref, struct ext4_dir_en *de) {`

************************************************************************

---

## `static int ext4_dir_iterator_set(struct ext4_dir_iter *it, uint32_t block_size) {`

Do some checks before returning iterator.

- **`it`**: Iterator to be checked
- **`block_size`**: Size of data block
- **Returns**: Error code


---

## `static int ext4_dir_iterator_seek(struct ext4_dir_iter *it, uint64_t pos) {`

Seek to next valid directory entry.
Here can be jumped to the next data block.

- **`it`**: Initialized iterator
- **`pos`**: Position of the next entry
- **Returns**: Error code


---

