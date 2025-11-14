# include/mem/alloc/alloc.h

## `typedef void *(*cb_reqmem_t)(void *ptr, size_t size);`


\brief 请求内存的回调函数

除非 ptr 为 NULL, 否则因分配从 ptr 开始的 size 大小的内存
除非 [ptr 处已被占用] 或 [ptr 为 NULL 时内存已满]，否则不应该返回 NULL

多分配区模式下请求的 4k 内存必须符合 4k 对齐
多分配区模式下请求的 2M 内存必须符合 2M 对齐

\param ptr      上一次返回的内存尾地址
\param size     请求的内存大小
\return 分配的内存地址


---

## `typedef void (*cb_delmem_t)(void *ptr, size_t size);`


\brief 释放内存的回调函数

\param ptr      要释放的内存地址
\param size     要释放的内存大小


---

## `typedef struct sized_mpool {`


\brief 指定元素大小的内存池



---

## `void sized_mpool_init(sized_mpool_t pool, void *ptr, size_t bsize, size_t len);`


\brief 初始化一个内部元素大小固定的内存池

\param pool     内存池
\param ptr      内存区指针
\param bsize    池中元素的大小，必须大于等于 sizeof(size_t)
\param len      池中元素的个数


---

## `void *sized_mpool_alloc(sized_mpool_t pool);`


\brief 从内存池中分配

\param pool     内存池
\return value


---

## `void sized_mpool_free(sized_mpool_t pool, void *ptr);`


\brief 释放内存池中的内存

\param pool     内存池
\param ptr      param


---

## `bool sized_mpool_inpool(sized_mpool_t pool, void *ptr);`


\brief 判断指定的内存地址是否在内存池中

\param pool     内存池
\param ptr      要判断的地址
\return 指定的内存地址是否在内存池中


---

## `typedef struct mpool {`


\brief 内存池



---

## `bool mpool_init(mpool_t pool, void *ptr, size_t size);`


\brief 初始化一个内存池

\param pool     内存池
\param ptr      内存区指针
\param size     内存区总大小
\return 初始化是否成功 (即输入参数是否合法)


---

## `size_t mpool_alloced_size(mpool_t pool);`


\brief 返回已分配的内存总大小

\param pool     内存池
\return 已分配的内存总大小


---

## `void mpool_setcb(mpool_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem);`


\brief 设置内存池的回调函数

\param pool     内存池
\param reqmem   请求内存的回调函数
\param delmem   释放内存的回调函数


---

## `void *mpool_alloc(mpool_t pool, size_t size);`


\brief 从内存池中分配

\param pool     内存池
\param size     请求的内存大小
\return 分配的内存地址


---

## `void *mpool_aligned_alloc(mpool_t pool, size_t size, size_t align);`


\brief 从内存池中分配对齐的内存

\param pool     内存池
\param size     请求的内存大小
\param align    对齐大小
\return 分配的内存地址


---

## `void mpool_free(mpool_t pool, void *ptr);`


\brief 释放内存池中的内存

\param pool     内存池
\param ptr      要释放的内存地址


---

## `size_t mpool_msize(mpool_t pool, void *ptr);`


\brief 获取分配的内存的大小

\param pool     内存池
\param ptr      分配的内存地址
\return 分配的内存的大小


---

## `void *mpool_realloc(mpool_t pool, void *ptr, size_t newsize);`


\brief 从内存池中重新分配内存

\param pool     内存池
\param ptr      要重新分配的内存地址
\param newsize  新的内存大小
\return 重新分配的内存地址


---

## `void *mpool_aligned_realloc(mpool_t pool, void *ptr, size_t newsize, size_t align);`


\brief 从内存池中重新分配对齐的内存
!      注意 align 和第一次分配时的 align 必须相同

\param pool     内存池
\param ptr      要重新分配的内存地址
\param newsize  新的内存大小
\param align    对齐大小
\return 重新分配的内存地址


---

## `typedef struct mman_pool *mman_pool_t;`


\brief 内存分配区的块结构



---

## `typedef struct mman {`


\brief 内存管理器



---

