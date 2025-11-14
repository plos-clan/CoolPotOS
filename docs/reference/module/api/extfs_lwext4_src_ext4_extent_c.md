# extfs/lwext4/src/ext4_extent.c

## `uint32_t leaf_lo;`


Pointer to the physical block of the next
level. leaf or next index could be there
high 16 bits of physical block


---

## `static inline uint32_t ext4_extent_get_first_block(struct ext4_extent *extent) {`

Get logical number of the block covered by extent.

- **`extent`**: Extent to load number from
- **Returns**: Logical number of the first block covered by extent 

---

## `static inline void ext4_extent_set_first_block(struct ext4_extent *extent, uint32_t iblock) {`

Set logical number of the first block covered by extent.

- **`extent`**: Extent to set number to
- **`iblock`**: Logical number of the first block covered by extent 

---

## `static inline uint16_t ext4_extent_get_block_count(struct ext4_extent *extent) {`

Get number of blocks covered by extent.

- **`extent`**: Extent to load count from
- **Returns**: Number of blocks covered by extent 

---

## `static inline void ext4_extent_set_block_count(struct ext4_extent *extent, uint16_t count, bool unwritten) {`

Set number of blocks covered by extent.

- **`extent`**: Extent to load count from
- **`count`**: Number of blocks covered by extent
- **`unwritten`**: Whether the extent is unwritten or not 

---

## `static inline uint64_t ext4_extent_get_start(struct ext4_extent *extent) {`

Get physical number of the first block covered by extent.

- **`extent`**: Extent to load number
- **Returns**: Physical number of the first block covered by extent 

---

## `static inline void ext4_extent_set_start(struct ext4_extent *extent, uint64_t fblock) {`

Set physical number of the first block covered by extent.

- **`extent`**: Extent to load number
- **`fblock`**: Physical number of the first block covered by extent 

---

## `static inline uint32_t ext4_extent_index_get_first_block(struct ext4_extent_index *index) {`

Get logical number of the block covered by extent index.

- **`index`**: Extent index to load number from
- **Returns**: Logical number of the first block covered by extent index 

---

## `static inline void ext4_extent_index_set_first_block(struct ext4_extent_index *index, uint32_t iblock) {`

Set logical number of the block covered by extent index.

- **`index`**: Extent index to set number to
- **`iblock`**: Logical number of the first block covered by extent index 

---

## `static inline uint64_t ext4_extent_index_get_leaf(struct ext4_extent_index *index) {`

Get physical number of block where the child node is located.

- **`index`**: Extent index to load number from
- **Returns**: Physical number of the block with child node 

---

## `static inline void ext4_extent_index_set_leaf(struct ext4_extent_index *index, uint64_t fblock) {`

Set physical number of block where the child node is located.

- **`index`**: Extent index to set number to
- **`fblock`**: Ohysical number of the block with child node 

---

## `static inline uint16_t ext4_extent_header_get_magic(struct ext4_extent_header *header) {`

Get magic value from extent header.

- **`header`**: Extent header to load value from
- **Returns**: Magic value of extent header 

---

## `static inline void ext4_extent_header_set_magic(struct ext4_extent_header *header, uint16_t magic) {`

Set magic value to extent header.

- **`header`**: Extent header to set value to
- **`magic`**: Magic value of extent header 

---

## `static inline uint16_t ext4_extent_header_get_entries_count(struct ext4_extent_header *header) {`

Get number of entries from extent header

- **`header`**: Extent header to get value from
- **Returns**: Number of entries covered by extent header 

---

## `static inline void ext4_extent_header_set_entries_count(struct ext4_extent_header *header, uint16_t count) {`

Set number of entries to extent header

- **`header`**: Extent header to set value to
- **`count`**: Number of entries covered by extent header 

---

## `static inline uint16_t ext4_extent_header_get_max_entries_count(struct ext4_extent_header *header) {`

Get maximum number of entries from extent header

- **`header`**: Extent header to get value from
- **Returns**: Maximum number of entries covered by extent header 

---

## `static inline void ext4_extent_header_set_max_entries_count(struct ext4_extent_header *header, uint16_t max_count) {`

Set maximum number of entries to extent header

- **`header`**: Extent header to set value to
- **`max_count`**: Maximum number of entries covered by extent header 

---

## `static inline uint16_t ext4_extent_header_get_depth(struct ext4_extent_header *header) {`

Get depth of extent subtree.

- **`header`**: Extent header to get value from
- **Returns**: Depth of extent subtree 

---

## `static inline void ext4_extent_header_set_depth(struct ext4_extent_header *header, uint16_t depth) {`

Set depth of extent subtree.

- **`header`**: Extent header to set value to
- **`depth`**: Depth of extent subtree 

---

## `static inline uint32_t ext4_extent_header_get_generation(struct ext4_extent_header *header) {`

Get generation from extent header

- **`header`**: Extent header to get value from
- **Returns**: Generation 

---

## `static inline void ext4_extent_header_set_generation(struct ext4_extent_header *header, uint32_t generation) {`

Set generation to extent header

- **`header`**: Extent header to set value to
- **`generation`**: Generation 

---

