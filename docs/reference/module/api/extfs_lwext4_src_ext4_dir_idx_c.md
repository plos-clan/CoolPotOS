# extfs/lwext4/src/ext4_dir_idx.c

## `static inline uint8_t ext4_dir_dx_rinfo_get_hash_version(struct ext4_dir_idx_rinfo *ri) {`

Get hash version used in directory index.

- **`ri`**: Pointer to root info structure of index
- **Returns**: Hash algorithm version


---

## `static inline void ext4_dir_dx_rinfo_set_hash_version(struct ext4_dir_idx_rinfo *ri, uint8_t v) {`

Set hash version, that will be used in directory index.

- **`ri`**: Pointer to root info structure of index
- **`v`**: Hash algorithm version


---

## `static inline uint8_t ext4_dir_dx_rinfo_get_info_length(struct ext4_dir_idx_rinfo *ri) {`

Get length of root_info structure in bytes.

- **`ri`**: Pointer to root info structure of index
- **Returns**: Length of the structure


---

## `static inline void ext4_dir_dx_root_info_set_info_length(struct ext4_dir_idx_rinfo *ri, uint8_t len) {`

Set length of root_info structure in bytes.

- **`ri`**: Pointer to root info structure of index
- **`len`**: Length of the structure


---

## `static inline uint8_t ext4_dir_dx_rinfo_get_indirect_levels(struct ext4_dir_idx_rinfo *ri) {`

Get number of indirect levels of HTree.

- **`ri`**: Pointer to root info structure of index
- **Returns**: Height of HTree (actually only 0 or 1)


---

## `static inline void ext4_dir_dx_rinfo_set_indirect_levels(struct ext4_dir_idx_rinfo *ri, uint8_t l) {`

Set number of indirect levels of HTree.

- **`ri`**: Pointer to root info structure of index
- **`l`**: Height of HTree (actually only 0 or 1)


---

## `static inline uint16_t ext4_dir_dx_climit_get_limit(struct ext4_dir_idx_climit *climit) {`

Get maximum number of index node entries.

- **`climit`**: Pointer to counlimit structure
- **Returns**: Maximum of entries in node


---

## `static inline void ext4_dir_dx_climit_set_limit(struct ext4_dir_idx_climit *climit, uint16_t limit) {`

Set maximum number of index node entries.

- **`climit`**: Pointer to counlimit structure
- **`limit`**: Maximum of entries in node


---

## `static inline uint16_t ext4_dir_dx_climit_get_count(struct ext4_dir_idx_climit *climit) {`

Get current number of index node entries.

- **`climit`**: Pointer to counlimit structure
- **Returns**: Number of entries in node


---

## `static inline void ext4_dir_dx_climit_set_count(struct ext4_dir_idx_climit *climit, uint16_t count) {`

Set current number of index node entries.

- **`climit`**: Pointer to counlimit structure
- **`count`**: Number of entries in node


---

## `static inline uint32_t ext4_dir_dx_entry_get_hash(struct ext4_dir_idx_entry *entry) {`

Get hash value of index entry.

- **`entry`**: Pointer to index entry
- **Returns**: Hash value


---

## `static inline void ext4_dir_dx_entry_set_hash(struct ext4_dir_idx_entry *entry, uint32_t hash) {`

Set hash value of index entry.

- **`entry`**: Pointer to index entry
- **`hash`**: Hash value


---

## `static inline uint32_t ext4_dir_dx_entry_get_block(struct ext4_dir_idx_entry *entry) {`

Get block address where child node is located.

- **`entry`**: Pointer to index entry
- **Returns**: Block address of child node


---

## `static inline void ext4_dir_dx_entry_set_block(struct ext4_dir_idx_entry *entry, uint32_t block) {`

Set block address where child node is located.

- **`entry`**: Pointer to index entry
- **`block`**: Block address of child node


---

## `struct ext4_dx_sort_entry {`

Sort entry item.

---

## `int ext4_dir_dx_init(struct ext4_inode_ref *dir, struct ext4_inode_ref *parent) {`

************************************************************************

---

## `static int ext4_dir_hinfo_init(struct ext4_hash_info *hinfo, struct ext4_block *root_block, struct ext4_sblock *sb, size_t name_len, const char *name) {`

Initialize hash info structure necessary for index operations.

- **`hinfo`**: Pointer to hinfo to be initialized
- **`root_block`**: Root block (number 0) of index
- **`sb`**: Pointer to superblock
- **`name_len`**: Length of name to be computed hash value from
- **`name`**: Name to be computed hash value from
- **Returns**: Standard error code


---

## `static int ext4_dir_dx_get_leaf(struct ext4_hash_info *hinfo, struct ext4_inode_ref *inode_ref, struct ext4_block *root_block, struct ext4_dir_idx_block **dx_block, struct ext4_dir_idx_block *dx_blocks) {`

Walk through index tree and load leaf with corresponding hash value.

- **`hinfo`**: Initialized hash info structure
- **`inode_ref`**: Current i-node
- **`root_block`**: Root block (iblock 0), where is root node located
- **`dx_block`**: Pointer to leaf node in dx_blocks array
- **`dx_blocks`**: Array with the whole path from root to leaf
- **Returns**: Standard error code


---

## `static int ext4_dir_dx_next_block(struct ext4_inode_ref *inode_ref, uint32_t hash, struct ext4_dir_idx_block *dx_block, struct ext4_dir_idx_block *dx_blocks) {`

Check if the the next block would be checked during entry search.

- **`inode_ref`**: Directory i-node
- **`hash`**: Hash value to check
- **`dx_block`**: Current block
- **`dx_blocks`**: Array with path from root to leaf node
- **Returns**: Standard Error code


---

## `static int ext4_dir_dx_entry_comparator(const void *arg1, const void *arg2) {`

Compare function used to pass in quicksort implementation.
It can compare two entries by hash value.

- **`arg1`**: First entry
- **`arg2`**: Second entry


- **Returns**: Classic compare result
(0: equal, -1: arg1 < arg2, 1: arg1 > arg2)


---

## `static void ext4_dir_dx_insert_entry(struct ext4_inode_ref *inode_ref __unused, struct ext4_dir_idx_block *index_block, uint32_t hash, uint32_t iblock) {`

Insert new index entry to block.
Note that space for new entry must be checked by caller.

- **`inode_ref`**: Directory i-node
- **`index_block`**: Block where to insert new entry
- **`hash`**: Hash value covered by child node
- **`iblock`**: Logical number of child block



---

## `static int ext4_dir_dx_split_data(struct ext4_inode_ref *inode_ref, struct ext4_hash_info *hinfo, struct ext4_block *old_data_block, struct ext4_dir_idx_block *index_block, struct ext4_block *new_data_block) {`

Split directory entries to two parts preventing node overflow.

- **`inode_ref`**: Directory i-node
- **`hinfo`**: Hash info
- **`old_data_block`**: Block with data to be split
- **`index_block`**: Block where index entries are located
- **`new_data_block`**: Output value for newly allocated data block


---

## `static int ext4_dir_dx_split_index(struct ext4_inode_ref *ino_ref, struct ext4_dir_idx_block *dx_blks, struct ext4_dir_idx_block *dxb, struct ext4_dir_idx_block **new_dx_block) {`

Split index node and maybe some parent nodes in the tree hierarchy.

- **`ino_ref`**: Directory i-node
- **`dx_blks`**: Array with path from root to leaf node
- **`dxb`**: Leaf block to be split if needed
- **Returns**: Error code


---

