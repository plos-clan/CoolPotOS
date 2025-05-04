#pragma once

#include "ctype.h"

// 内存分配时大小和返回指针的对齐 (按照两倍字长)
#define MALLOC_PADDING(size) (((size) + 2 * sizeof(size_t) - 1) & ~(2 * sizeof(size_t) - 1))

#define SIZE_4k  ((size_t)4096)
#define SIZE_16k ((size_t)16384)
#define SIZE_4M  ((size_t)(4 * 1024 * 1024))
#define SIZE_2M  ((size_t)(2 * 1024 * 1024))
#define SIZE_1G  ((size_t)(1024 * 1024 * 1024))

#ifndef ALLOC_LARGE_BLK_SIZE
#    define ALLOC_LARGE_BLK_SIZE ((size_t)16384)
#endif

#ifndef PAGE_SIZE
#    define PAGE_SIZE ((size_t)4096)
#endif

//* ----------------------------------------------------------------------------------------------------
//& 标准库内存分配函数

// 这些函数应该由 libc 本体提供
// 而在没有 libc 的情况下管理内存请使用下方的自定义内存分配函数

// 分配器通用属性
#define ALLOC __THROW __wur __attr_dealloc(free, 1) __attr_dealloc(realloc, 1)

void *malloc(size_t size)

    ALLOC

    __attr_malloc __attr_allocsize(1) ownership_returns(malloc);

void *xmalloc(size_t size)

    ALLOC

    __attr_malloc __attr_allocsize(

        1) ownership_returns(malloc) __attr(returns_nonnull);

void free(void *ptr)

    __THROW ownership_takes(malloc,

                            1);

void *calloc(size_t n, size_t size)

    ALLOC

    __attr_malloc __attr_allocsize(

        1, 2) ownership_returns(malloc);

void *realloc(void *ptr, size_t newsize)

    ALLOC __attr_allocsize(2)

        ownership_takes(malloc,

                        1) ownership_returns(malloc);

void *reallocarray(void *ptr, size_t n, size_t size)

    __THROW __wur __attr_allocsize(

        2, 3);

void *aligned_alloc(size_t align, size_t size)

    __THROW __attr_malloc __attr_allocsize(2);

size_t malloc_usable_size(void *ptr) __THROW;
void  *memalign(size_t align, size_t size)

    __THROW __attr_malloc __attr_allocsize(2);

int posix_memalign(void **mem_p, size_t align, size_t sie)

    __THROW __nnull(

        1) __wur;

void *pvalloc(size_t size)

    __THROW __attr_malloc __attr_allocsize(1);

void *valloc(size_t size)

    __THROW __attr_malloc __attr_allocsize(1);

#undef ALLOC

//* ----------------------------------------------------------------------------------------------------
//& 自定义内存分配

/**
 *\brief 请求内存的回调函数
 *
 * 除非 ptr 为 NULL, 否则因分配从 ptr 开始的 size 大小的内存
 * 除非 [ptr 处已被占用] 或 [ptr 为 NULL 时内存已满]，否则不应该返回 NULL
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
void sized_mpool_init(sized_mpool_t pool, void *ptr, size_t bsize, size_t len);

/**
 *\brief 从内存池中分配
 *
 *\param pool     内存池
 *\return value
 */
void *sized_mpool_alloc(sized_mpool_t pool);

/**
 *\brief 释放内存池中的内存
 *
 *\param pool     内存池
 *\param ptr      param
 */
void sized_mpool_free(sized_mpool_t pool, void *ptr);

/**
 *\brief 判断指定的内存地址是否在内存池中
 *
 *\param pool     内存池
 *\param ptr      要判断的地址
 *\return 指定的内存地址是否在内存池中
 */
bool sized_mpool_inpool(sized_mpool_t pool, void *ptr);

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
bool mpool_init(mpool_t pool, void *ptr, size_t size);

// 占用的内存总大小
size_t mpool_total_size(mpool_t pool);

/**
 *\brief 返回已分配的内存总大小
 *
 *\param pool     内存池
 *\return 已分配的内存总大小
 */
size_t mpool_alloced_size(mpool_t pool);

/**
 *\brief 设置内存池的回调函数
 *
 *\param pool     内存池
 *\param reqmem   请求内存的回调函数
 *\param delmem   释放内存的回调函数
 */
void mpool_setcb(mpool_t pool, cb_reqmem_t reqmem, cb_delmem_t delmem);

/**
 *\brief 从内存池中分配
 *
 *\param pool     内存池
 *\param size     请求的内存大小
 *\return 分配的内存地址
 */
void *mpool_alloc(mpool_t pool, size_t size);

/**
 *\brief 从内存池中分配对齐的内存
 *
 *\param pool     内存池
 *\param size     请求的内存大小
 *\param align    对齐大小
 *\return 分配的内存地址
 */
void *mpool_aligned_alloc(mpool_t pool, size_t size, size_t align);

/**
 *\brief 释放内存池中的内存
 *
 *\param pool     内存池
 *\param ptr      要释放的内存地址
 */
void mpool_free(mpool_t pool, void *ptr);

/**
 *\brief 获取分配的内存的大小
 *
 *\param pool     内存池
 *\param ptr      分配的内存地址
 *\return 分配的内存的大小
 */
size_t mpool_msize(mpool_t pool, void *ptr);

/**
 *\brief 从内存池中重新分配内存
 *
 *\param pool     内存池
 *\param ptr      要重新分配的内存地址
 *\param newsize  新的内存大小
 *\return 重新分配的内存地址
 */
void *mpool_realloc(mpool_t pool, void *ptr, size_t newsize);

/**
 *\brief 从内存池中重新分配对齐的内存
 *!      注意 align 和第一次分配时的 align 必须相同
 *
 *\param pool     内存池
 *\param ptr      要重新分配的内存地址
 *\param newsize  新的内存大小
 *\param align    对齐大小
 *\return 重新分配的内存地址
 */
void *mpool_aligned_realloc(mpool_t pool, void *ptr, size_t newsize, size_t align);

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

bool mman_init(mman_t man, void *ptr, size_t size);

size_t mman_total_size(mman_t man);

size_t mman_alloced_size(mman_t man);

void mman_setcb(mman_t man, cb_reqmem_t reqmem, cb_delmem_t delmem);

void *mman_alloc(mman_t man, size_t size);

void *mman_aligned_alloc(mman_t man, size_t size, size_t align);

void mman_free(mman_t man, void *ptr);

size_t mman_msize(mman_t man, void *ptr);

void *mman_realloc(mman_t man, void *ptr, size_t newsize);

void *mman_aligned_realloc(mman_t man, void *ptr, size_t newsize, size_t align);
