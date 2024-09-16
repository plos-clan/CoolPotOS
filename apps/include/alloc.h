#pragma once

#define ALLOC_FORCE_2M_PAGE 0

#define ALLOC_LARGE_BLK_SIZE 16384

#include <stddef.h>

#define true 1
#define false 0
#define bool _Bool
#define auto __auto_type

#define finline static inline __attribute__((always_inline))
#define dlimport
#define dlexport extern

#define __THROW
#define __wur __attribute__((warn_unused_result))
#define __attr_malloc __attribute__((malloc))
#define __attr_allocsize(...) __attribute__((allocsize(__VA_ARGS__)))
#define __nnull(...) __attribute__((nonnull(__VA_ARGS__)))
#define __attr(...) __attribute__((__VA_ARGS__))
#define null NULL

// 内存分配时大小和返回指针的对齐 (按照两倍字长)
#define MALLOC_PADDING(size) (((size) + 2 * sizeof(size_t) - 1) & ~(2 * sizeof(size_t) - 1))

// TODO 将宏定义移至 config

// 默认认为分页大小是 4k
// 如果是 2M 请调整为 2097152
#define PAGE_SIZE ((size_t)4096)

#define SIZE_4k ((size_t)4096)
#define SIZE_2M ((size_t)(2 * 1024 * 1024))

//* ----------------------------------------------------------------------------------------------------
//& 标准库内存分配函数

dlimport void  *malloc(size_t size) __THROW __attr_malloc __attr_allocsize(1);
dlimport void  *xmalloc(size_t size) __THROW __attr_malloc __attr_allocsize(1);
dlimport void   free(void *ptr) __THROW;
dlimport void  *calloc(size_t n, size_t size) __THROW __attr_malloc __attr_allocsize(1, 2);
dlimport void  *realloc(void *ptr, size_t newsize) __THROW __wur __attr_allocsize(2);
dlimport void  *reallocarray(void *ptr, size_t n, size_t size) __THROW __wur __attr_allocsize(2, 3);
dlimport void  *aligned_alloc(size_t align, size_t size) __THROW __attr_malloc __attr_allocsize(2);
dlimport size_t malloc_usable_size(void *ptr) __THROW;
dlimport void  *memalign(size_t align, size_t size) __THROW __attr_malloc __attr_allocsize(2);
dlimport int    posix_memalign(void **mem_p, size_t align, size_t sie) __THROW __nnull(1) __wur;
dlimport void  *pvalloc(size_t size) __THROW __attr_malloc __attr_allocsize(1);
dlimport void  *valloc(size_t size) __THROW __attr_malloc __attr_allocsize(1);

//* ----------------------------------------------------------------------------------------------------
//& 自定义内存分配

/**
 *\brief 请求内存的回调函数
 *
 * 除非 ptr 为 null, 否则因分配从 ptr 开始的 size 大小的内存
 * 除非 [ptr 处已被占用] 或 [ptr 为 null 时内存已满]，否则不应该返回 null
 *
 * 多分配区模式下请求的 4k 内存必须符合 4k 对齐
 * 多分配区模式下请求的 2M 内存必须符合 2M 对齐
 *
 *\param ptr      上一次返回的内存尾地址
 *\param size     请求的内存大小
 *\return 分配的内存地址
 */
typedef void *(*cb_reqmem_t)(void *ptr, size_t size);
/**
 *\brief 释放内存的回调函数
 *
 *\param ptr      要释放的内存地址
 *\param size     要释放的内存大小
 */
typedef void (*cb_delmem_t)(void *ptr, size_t size);

//* ----------------------------------------------------------------------------------------------------
//& 空闲链表

#define FREELIST_NUM 8

typedef struct freelist *freelist_t;

typedef freelist_t freelists_t[FREELIST_NUM];

//* ----------------------------------------------------------------------------------------------------
//& 指定元素大小的内存池

/**
 *\brief 指定元素大小的内存池
 *
 */
typedef struct sized_mpool {
  void  *ptr;      // 指向内存区的指针
  size_t size;     // 内存区总大小
  size_t bsize;    // 每个元素的大小
  size_t len;      // 总共能容纳的元素个数
  size_t nalloced; // 已分配计数
  void  *freelist; // 空闲列表
} *sized_mpool_t;

/**
 *\brief 初始化一个内部元素大小固定的内存池
 *
 *\param pool     内存池
 *\param ptr      内存区指针
 *\param bsize    池中元素的大小，必须大于等于 sizeof(size_t)
 *\param len      池中元素的个数
 */
dlexport void sized_mpool_init(sized_mpool_t pool, void *ptr, size_t bsize, size_t len);

/**
 *\brief 从内存池中分配
 *
 *\param pool     内存池
 *\return value
 */
dlexport void *sized_mpool_alloc(sized_mpool_t pool);

/**
 *\brief 
 *
 *\param pool     内存池
 *\param ptr      param
 */
dlexport void sized_mpool_free(sized_mpool_t pool, void *ptr);

/**
 *\brief 判断指定的内存地址是否在内存池中
 *
 *\param pool     内存池
 *\param ptr      要判断的地址
 *\return 指定的内存地址是否在内存池中
 */
dlexport bool sized_mpool_inpool(sized_mpool_t pool, void *ptr);

//* ----------------------------------------------------------------------------------------------------
//& 大块内存管理

#define LARGEBLKLIST_NUM 16

typedef struct large_blk *large_blk_t;

typedef large_blk_t large_blks_t[LARGEBLKLIST_NUM];

//* ----------------------------------------------------------------------------------------------------
//& 内存池

/**
 *\brief 内存池
 *
 */
typedef struct mpool {
  void       *ptr;          // 指向内存区的指针
  size_t      size;         // 内存区总大小
  size_t      alloced_size; // 已分配的内存大小
  cb_reqmem_t cb_reqmem;    // 请求内存的回调函数
  cb_delmem_t cb_delmem;    // 释放内存的回调函数
  freelist_t  large_blk;    // 大块内存的空闲链表
  freelists_t freed;        // 小块内存的空闲链表 (组)
} *mpool_t;

/**
 *\brief 初始化一个内存池
 *
 *\param pool     内存池
 *\param ptr      内存区指针
 *\param size     内存区总大小
 *\return 初始化是否成功 (即输入参数是否合法)
 */
dlimport bool mpool_init(mpool_t pool, void *ptr, size_t size);

// 占用的内存总大小
dlimport size_t mpool_total_size(mpool_t pool);

/**
 *\brief 返回已分配的内存总大小
 *
 *\param pool     内存池
 *\return 已分配的内存总大小
 */
dlimport size_t mpool_alloced_size(mpool_t pool);

/**
 *\brief 设置内存池的回调函数
 *
 *\param pool     内存池
 *\param reqmem   请求内存的回调函数
 *\param delmem   释放内存的回调函数
 */
dlimport void mpool_setcb(mpool_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem);

/**
 *\brief 从内存池中分配
 *
 *\param pool     内存池
 *\param size     请求的内存大小
 *\return 分配的内存地址
 */
dlimport void *mpool_alloc(mpool_t pool, size_t size);

/**
 *\brief 释放内存池中的内存
 *
 *\param pool     内存池
 *\param ptr      要释放的内存地址
 */
dlimport void mpool_free(mpool_t pool, void *ptr);

/**
 *\brief 获取分配的内存的大小
 *
 *\param pool     内存池
 *\param ptr      分配的内存地址
 *\return 分配的内存的大小
 */
dlimport size_t mpool_msize(mpool_t pool, void *ptr);

dlimport void *mpool_realloc(mpool_t pool, void *ptr, size_t newsize);

//* ----------------------------------------------------------------------------------------------------
//& 内存管理器

/**
 *\brief 内存分配区的块结构
 *
 */
typedef struct mman_pool *mman_pool_t;
struct mman_pool {
  void       *ptr;          // 指向内存区的指针
  size_t      alloced_size; // 已分配的内存大小
  mman_pool_t next;         // 下一个内存池
};

/**
 *\brief 内存管理器
 *
 */
typedef struct mman {
  struct mman_pool main;         // 主分配区 (后接子分配区)
  size_t           size;         // 内存区总大小
  size_t           alloced_size; // 已分配的内存大小
  cb_reqmem_t      cb_reqmem;    // 请求内存的回调函数
  cb_delmem_t      cb_delmem;    // 释放内存的回调函数
  freelist_t       large_blk;    // 大块内存的空闲链表
  freelists_t      freed;        // 小块内存的空闲链表 (组)
  large_blks_t     large;        //
} *mman_t;

dlimport bool mman_init(mman_t pool, void *ptr, size_t size);

dlimport size_t mman_total_size(mman_t pool);

dlimport size_t mman_alloced_size(mman_t pool);

dlimport void mman_setcb(mman_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem);

dlimport void *mman_alloc(mman_t pool, size_t size);

dlimport void mman_free(mman_t pool, void *ptr);

dlimport size_t mman_msize(mman_t pool, void *ptr);

dlimport void *mman_realloc(mman_t man, void *ptr, size_t newsize);

