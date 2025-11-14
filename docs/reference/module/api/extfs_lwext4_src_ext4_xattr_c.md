# extfs/lwext4/src/ext4_xattr.c

## `/* Extended Attribute(EA) */ /* Magic value in attribute blocks */ # define EXT4_XATTR_MAGIC 0xEA020000 /* Maximum number of references to one attribute block */ # define EXT4_XATTR_REFCOUNT_MAX 1024 /* Name indexes */ # define EXT4_XATTR_INDEX_USER 1 # define EXT4_XATTR_INDEX_POSIX_ACL_ACCESS 2 # define EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT 3 # define EXT4_XATTR_INDEX_TRUSTED 4 # define EXT4_XATTR_INDEX_LUSTRE 5 # define EXT4_XATTR_INDEX_SECURITY 6 # define EXT4_XATTR_INDEX_SYSTEM 7 # define EXT4_XATTR_INDEX_RICHACL 8 # define EXT4_XATTR_INDEX_ENCRYPTION 9 # define EXT4_XATTR_PAD_BITS 2 # define EXT4_XATTR_PAD (1 << EXT4_XATTR_PAD_BITS) # define EXT4_XATTR_ROUND (EXT4_XATTR_PAD - 1) # define EXT4_XATTR_LEN(name_len) \ (((name_len) + EXT4_XATTR_ROUND + sizeof(struct ext4_xattr_entry)) & ~EXT4_XATTR_ROUND) # define EXT4_XATTR_NEXT(entry) \ ((struct ext4_xattr_entry *)((char *)(entry) + EXT4_XATTR_LEN((entry)->e_name_len))) # define EXT4_XATTR_SIZE(size) (((size) + EXT4_XATTR_ROUND) & ~EXT4_XATTR_ROUND) # define EXT4_XATTR_NAME(entry) ((char *)((entry) + 1)) # define EXT4_XATTR_IHDR(sb, raw_inode) \ ((struct ext4_xattr_ibody_header *)((char *)raw_inode + EXT4_GOOD_OLD_INODE_SIZE + \ ext4_inode_get_extra_isize(sb, raw_inode))) # define EXT4_XATTR_IFIRST(hdr) ((struct ext4_xattr_entry *)((hdr) + 1)) # define EXT4_XATTR_BHDR(block) ((struct ext4_xattr_header *)((block)->data)) # define EXT4_XATTR_ENTRY(ptr) ((struct ext4_xattr_entry *)(ptr)) # define EXT4_XATTR_BFIRST(block) EXT4_XATTR_ENTRY(EXT4_XATTR_BHDR(block) + 1) # define EXT4_XATTR_IS_LAST_ENTRY(entry) (*(uint32_t *)(entry) == 0) # define EXT4_ZERO_XATTR_VALUE ((void *)-1) # pragma pack(push, 1) struct ext4_xattr_header {`


@file  ext4_xattr.c
Extended Attribute Manipulation


---

## `static int ext4_xattr_set_entry(struct ext4_xattr_info *i, struct ext4_xattr_search *s, bool dry_run) {`


Insert/Remove/Modify the given entry


- **`i`**: The information of the given EA entry
- **`s`**: Search context block
- **`dry_run`**: Do not modify the content of the buffer


- **Returns**: Return EOK when finished, ENOSPC when there is no enough space


---

## `static void ext4_xattr_find_entry(struct ext4_xattr_info *i, struct ext4_xattr_search *s) {`


Find the entry according to given information


- **`i`**: The information of the EA entry to be found,
including name_index, name and the length of name

- **`s`**: Search context block


---

## `static bool ext4_xattr_is_block_valid(struct ext4_inode_ref *inode_ref, struct ext4_block *block) {`


Check whether the xattr block's content is valid


- **`inode_ref`**: Inode reference
- **`block`**: The block buffer to be validated


- **Returns**: true if @block is valid, false otherwise.


---

## `static bool ext4_xattr_is_ibody_valid(struct ext4_inode_ref *inode_ref) {`


Check whether the inode buffer's content is valid


- **`inode_ref`**: Inode reference


- **Returns**: true if the inode buffer is valid, false otherwise.


---

## `struct ext4_xattr_finder {`


An EA entry finder for inode buffer


---

## `struct ext4_xattr_info i;`


The information of the EA entry to be find


---

## `struct ext4_xattr_search s;`


Search context block of the current search


---

## `struct ext4_inode_ref *inode_ref;`


Inode reference to the corresponding inode


---

## `static void ext4_xattr_block_initialize(struct ext4_inode_ref *inode_ref, struct ext4_block *block) {`


Initialize a given xattr block


- **`inode_ref`**: Inode reference
- **`block`**: xattr block buffer


---

## `static int ext4_xattr_block_find_entry(struct ext4_inode_ref *inode_ref, struct ext4_xattr_finder *finder, struct ext4_block *block) {`


Find an EA entry inside a xattr block


- **`inode_ref`**: Inode reference
- **`finder`**: The caller-provided finder block with
information filled

- **`block`**: The block buffer to be looked into


- **Returns**: Return EOK no matter the entry is found or not.
If the IO operation or the buffer validation failed,
return other value.


---

## `static int ext4_xattr_ibody_find_entry(struct ext4_inode_ref *inode_ref, struct ext4_xattr_finder *finder) {`


Find an EA entry inside an inode's extra space


- **`inode_ref`**: Inode reference
- **`finder`**: The caller-provided finder block with
information filled


- **Returns**: Return EOK no matter the entry is found or not.
If the IO operation or the buffer validation failed,
return other value.


---

## `static int ext4_xattr_try_alloc_block(struct ext4_inode_ref *inode_ref) {`


Try to allocate a block holding EA entries.


- **`inode_ref`**: Inode reference


- **Returns**: Error code


---

## `static void ext4_xattr_try_free_block(struct ext4_inode_ref *inode_ref) {`


Try to free a block holding EA entries.


- **`inode_ref`**: Inode reference


- **Returns**: Error code


---

## `int ext4_xattr_list(struct ext4_inode_ref *inode_ref, struct ext4_xattr_list_entry *list, size_t *list_len) {`


Put a list of EA entries into a caller-provided buffer
In order to make sure that @list buffer can fit in the data,
the routine should be called twice.


- **`inode_ref`**: Inode reference
- **`list`**: A caller-provided buffer to hold a list of EA entries.
If list == NULL, list_len will contain the size of
the buffer required to hold these entries

- **`list_len`**: The length of the data written to @list
- **Returns**: Error code


---

## `int ext4_xattr_get(struct ext4_inode_ref *inode_ref, uint8_t name_index, const char *name, size_t name_len, void *buf, size_t buf_len, size_t *data_len) {`


Query EA entry's value with given name-index and name


- **`inode_ref`**: Inode reference
- **`name_index`**: Name-index
- **`name`**: Name of the EA entry to be queried
- **`name_len`**: Length of name in bytes
- **`buf`**: Output buffer to hold content
- **`buf_len`**: Output buffer's length
- **`data_len`**: The length of data of the EA entry found


- **Returns**: Error code


---

## `static int ext4_xattr_copy_new_block(struct ext4_inode_ref *inode_ref, struct ext4_block *block, struct ext4_block *new_block, ext4_fsblk_t *orig_block, bool *allocated) {`


Try to copy the content of an xattr block to a newly-allocated
block. If the operation fails, the block buffer provided by
caller will be freed


- **`inode_ref`**: Inode reference
- **`block`**: The block buffer reference
- **`new_block`**: The newly-allocated block buffer reference
- **`orig_block`**: The block number of @block
- **`allocated`**: a new block is allocated


- **Returns**: Error code


---

## `int ext4_xattr_remove(struct ext4_inode_ref *inode_ref, uint8_t name_index, const char *name, size_t name_len) {`


Given an EA entry's name, remove the EA entry


- **`inode_ref`**: Inode reference
- **`name_index`**: Name-index
- **`name`**: Name of the EA entry to be removed
- **`name_len`**: Length of name in bytes


- **Returns**: Error code


---

## `static int ext4_xattr_block_set(struct ext4_inode_ref *inode_ref, struct ext4_xattr_info *i, bool no_insert) {`


Insert/overwrite an EA entry into/in a xattr block


- **`inode_ref`**: Inode reference
- **`i`**: The information of the given EA entry


- **Returns**: Error code


---

## `static int ext4_xattr_block_remove(struct ext4_inode_ref *inode_ref, struct ext4_xattr_info *i) {`


Remove an EA entry from a xattr block


- **`inode_ref`**: Inode reference
- **`i`**: The information of the given EA entry


- **Returns**: Error code


---

## `int ext4_xattr_set(struct ext4_inode_ref *inode_ref, uint8_t name_index, const char *name, size_t name_len, const void *value, size_t value_len) {`


Insert an EA entry into a given inode reference


- **`inode_ref`**: Inode reference
- **`name_index`**: Name-index
- **`name`**: Name of the EA entry to be inserted
- **`name_len`**: Length of name in bytes
- **`value`**: Input buffer to hold content
- **`value_len`**: Length of input content


- **Returns**: Error code


---

