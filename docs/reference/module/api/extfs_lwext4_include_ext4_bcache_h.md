# extfs/lwext4/include/ext4_bcache.h

## `#ifndef EXT4_BCACHE_H_ #define EXT4_BCACHE_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_bcache.h
Block cache allocator.


---

## `struct ext4_block {`

Single block descriptor

---

## `uint64_t lb_id;`

Logical block ID

---

## `struct ext4_buf *buf;`

Buffer 

---

## `uint8_t *data;`

Data buffer.

---

## `struct ext4_buf {`

Single block descriptor

---

## `int flags;`

Flags

---

## `uint64_t lba;`

Logical block address

---

## `uint8_t *data;`

Data buffer.

---

## `uint32_t lru_prio;`

LRU priority. (unused) 

---

## `uint32_t lru_id;`

LRU id.

---

## `uint32_t refctr;`

Reference count table

---

## `struct ext4_bcache *bc;`

The block cache this buffer belongs to. 

---

## `bool on_dirty_list;`

Whether or not buffer is on dirty list.

---

## `RB_ENTRY(ext4_buf) lba_node;`

LBA tree node

---

## `RB_ENTRY(ext4_buf) lru_node;`

LRU tree node

---

## `SLIST_ENTRY(ext4_buf) dirty_node;`

Dirty list node

---

## `void (*end_write)(struct ext4_bcache *bc, struct ext4_buf *buf, int res, void *arg);`

Callback routine after a disk-write operation.

- **`bc`**: block cache descriptor
- **`buf`**: buffer descriptor
- **`standard`**: error code returned by bdev->bwrite()
- **`arg`**: argument passed to this routine

---

## `void *end_write_arg;`

argument passed to end_write() callback.

---

## `struct ext4_bcache {`

Block cache descriptor

---

## `uint32_t cnt;`

Item count in block cache

---

## `uint32_t itemsize;`

Item size in block cache

---

## `uint32_t lru_ctr;`

Last recently used counter

---

## `uint32_t ref_blocks;`

Currently referenced datablocks

---

## `uint32_t max_ref_blocks;`

Maximum referenced datablocks

---

## `struct ext4_blockdev *bdev;`

The blockdev binded to this block cache

---

## `bool dont_shake;`

The cache should not be shaked 

---

## `RB_HEAD(ext4_buf_lba, ext4_buf) lba_root;`

A tree holding all bufs

---

## `RB_HEAD(ext4_buf_lru, ext4_buf) lru_root;`

A tree holding unreferenced bufs

---

## `SLIST_HEAD(ext4_buf_dirty, ext4_buf) dirty_list;`

A singly-linked list holding dirty buffers

---

## `enum bcache_state_bits {`

buffer state bits

- BC♡UPTODATE: Buffer contains valid data.
- BC_DIRTY: Buffer is dirty.
- BC_FLUSH: Buffer will be immediately flushed,
when no one references it.
- BC_TMP: Buffer will be dropped once its refctr
reaches zero.


---

## `static inline void ext4_bcache_insert_dirty_node(struct ext4_bcache *bc, struct ext4_buf *buf) {`

Insert buffer to dirty cache list

- **`bc`**: block cache descriptor
- **`buf`**: buffer descriptor 

---

## `static inline void ext4_bcache_remove_dirty_node(struct ext4_bcache *bc, struct ext4_buf *buf) {`

Remove buffer to dirty cache list

- **`bc`**: block cache descriptor
- **`buf`**: buffer descriptor 

---

## `int ext4_bcache_init_dynamic(struct ext4_bcache *bc, uint32_t cnt, uint32_t itemsize);`

Dynamic initialization of block cache.

- **`bc`**: block cache descriptor
- **`cnt`**: items count in block cache
- **`itemsize`**: single item size (in bytes)
- **Returns**: standard error code

---

## `void ext4_bcache_cleanup(struct ext4_bcache *bc);`

Do cleanup works on block cache.

- **`bc`**: block cache descriptor.

---

## `int ext4_bcache_fini_dynamic(struct ext4_bcache *bc);`

Dynamic de-initialization of block cache.

- **`bc`**: block cache descriptor
- **Returns**: standard error code

---

## `struct ext4_buf *ext4_buf_lowest_lru(struct ext4_bcache *bc);`

Get a buffer with the lowest LRU counter in bcache.

- **`bc`**: block cache descriptor
- **Returns**: buffer with the lowest LRU counter

---

## `void ext4_bcache_drop_buf(struct ext4_bcache *bc, struct ext4_buf *buf);`

Drop unreferenced buffer from bcache.

- **`bc`**: block cache descriptor
- **`buf`**: buffer

---

## `void ext4_bcache_invalidate_buf(struct ext4_bcache *bc, struct ext4_buf *buf);`

Invalidate a buffer.

- **`bc`**: block cache descriptor
- **`buf`**: buffer

---

## `void ext4_bcache_invalidate_lba(struct ext4_bcache *bc, uint64_t from, uint32_t cnt);`

Invalidate a range of buffers.

- **`bc`**: block cache descriptor
- **`from`**: starting lba
- **`cnt`**: block counts

---

## `struct ext4_buf * ext4_bcache_find_get(struct ext4_bcache *bc, struct ext4_block *b, uint64_t lba);`

Find existing buffer from block cache memory.
Unreferenced block allocation is based on LRU
(Last Recently Used) algorithm.

- **`bc`**: block cache descriptor
- **`b`**: block to alloc
- **`lba`**: logical block address
- **Returns**: block cache buffer 

---

## `int ext4_bcache_alloc(struct ext4_bcache *bc, struct ext4_block *b, bool *is_new);`

Allocate block from block cache memory.
Unreferenced block allocation is based on LRU
(Last Recently Used) algorithm.

- **`bc`**: block cache descriptor
- **`b`**: block to alloc
- **`is_new`**: block is new (needs to be read)
- **Returns**: standard error code

---

## `int ext4_bcache_free(struct ext4_bcache *bc, struct ext4_block *b);`

Free block from cache memory (decrement reference counter).

- **`bc`**: block cache descriptor
- **`b`**: block to free
- **Returns**: standard error code

---

## `bool ext4_bcache_is_full(struct ext4_bcache *bc);`

Return a full status of block cache.

- **`bc`**: block cache descriptor
- **Returns**: full status

---

