# include/mem/alloc/block.h

## `static inline void blk_setsize(void *ptr, size_t size) {`


\brief 设置块的大小 (会清除块标记!)

\param ptr      块指针
\param size     块大小


---

## `static inline void *blk_trymerge(void *ptr, blk_detach_t detach, void *data) {`


\brief 尝试合并前后相邻块 (会清除块释放标记!)

\param ptr      块指针
\param detach   用于(可合并时)将相邻块从空闲链表中移除的回调
\param pool     内存池指针
\return 新的块指针


---

## `static inline void *blk_split(void *ptr, size_t size) {`


\brief 将一个块分割成两个 (会清除块释放标记!)

\param ptr      块指针
\param size     分割出第一个块的大小 (必须对齐到 2 倍字长!)
\return 第二个块的指针


---

