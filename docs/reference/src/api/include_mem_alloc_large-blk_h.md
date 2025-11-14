# include/mem/alloc/large-blk.h

## `static inline large_blk_t *large_blk_blokp(large_blks_t blks, void *ptr) {`


\brief 获取大内存块应该放入的空闲链表

\param blks     大内存块空闲链表 (组)
\param ptr      大内存块指针
\return 大块内存空闲链表指针


---

## `static inline void *large_blk_alloc(size_t size, large_blks_t blks, cb_reqmem_t reqmem, cb_delmem_t delmem) {`


\brief 分配大块内存

\param size     请求的内存大小 (保证其大于 ALLOC_LARGE_BLK_SIZE)
\param blks     大内存块空闲链表 (组)
\param reqmem   请求内存的回调函数
\param delmem   释放内存的回调函数
\return 分配的内存地址


---

## `static inline bool large_blk_free(large_blks_t blks, void *ptr, cb_delmem_t delmem) {`


\brief 释放大块内存
可以传入任何指针，非大内存块会返回 false

\param blks     大内存块空闲链表 (组)
\param ptr      大内存块指针
\param delmem   释放内存的回调函数
\return 是否释放成功


---

