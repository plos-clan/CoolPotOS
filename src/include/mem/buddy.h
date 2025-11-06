#pragma once

#define MAX_ORDER 20

// Zone 边界（物理地址）
#define ZONE_DMA_END   (16UL << 20) // 16MB
#define ZONE_DMA32_END (4UL << 30)  // 4GB

#define GFP_DMA    (1 << 0) // 必须从 ZONE_DMA 分配
#define GFP_DMA32  (1 << 1) // 可以从 ZONE_DMA32 分配
#define GFP_KERNEL (1 << 2) // 内核普通分配
#define GFP_ATOMIC (1 << 3) // 原子分配，不能睡眠
#define GFP_NOWAIT (1 << 4) // 不等待，快速失败

// 常用组合
#define GFP_KERNEL_NORMAL (GFP_KERNEL)
#define GFP_KERNEL_DMA    (GFP_KERNEL | GFP_DMA)
#define GFP_KERNEL_DMA32  (GFP_KERNEL | GFP_DMA32)

#define PG_reserved 0
#define PG_slab     1
#define PG_buddy    2
#define PG_compound 3
#define PG_head     4
#define PG_dirty    5
#define PG_lru      6

#define PCPU_CACHE_LOW  4
#define PCPU_CACHE_HIGH 32
#define PCPU_BATCH      8

// 页面标志操作
#define PageBuddy(page)      test_bit(PG_buddy, &(page)->flags)
#define SetPageBuddy(page)   set_bit(PG_buddy, &(page)->flags)
#define ClearPageBuddy(page) clear_bit(PG_buddy, &(page)->flags)

#define PageCompound(page)      test_bit(PG_compound, &(page)->flags)
#define SetPageCompound(page)   set_bit(PG_compound, &(page)->flags)
#define ClearPageCompound(page) clear_bit(PG_compound, &(page)->flags)

#define PageHead(page)      test_bit(PG_head, &(page)->flags)
#define SetPageHead(page)   set_bit(PG_head, &(page)->flags)
#define ClearPageHead(page) clear_bit(PG_head, &(page)->flags)

#define PageReserved(page)      test_bit(PG_reserved, &(page)->flags)
#define SetPageReserved(page)   set_bit(PG_reserved, &(page)->flags)
#define ClearPageReserved(page) clear_bit(PG_reserved, &(page)->flags)

#define pfn_to_page(pfn)   (&mem_map[(pfn) - min_pfn])
#define page_to_pfn(page)  ((uint64_t)((page) - mem_map) + min_pfn)
#define page_to_phys(page) (page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(phys) pfn_to_page((phys) >> PAGE_SHIFT)
#define virt_to_page(virt) phys_to_page(virt_to_phys(virt))
#define page_to_virt(page) phys_to_virt(page_to_phys(page))

#include "lock.h"
#include "types.h"

enum zone_type {
#if defined(__x86_64__)
    ZONE_DMA, // 0-16MB，用于传统 ISA DMA
#endif
    ZONE_DMA32,  // 0-4GB，用于 32 位 DMA
    ZONE_NORMAL, // 正常内存
    __MAX_NR_ZONES
};

extern const char *zone_names[__MAX_NR_ZONES];

enum zone_type pfn_to_zone_type(uint64_t pfn);

typedef struct {
    volatile int counter;
} atomic_t;

static inline void atomic_set(atomic_t *v, int i) {
    v->counter = i;
}

static inline int atomic_read(atomic_t *v) {
    return v->counter;
}

static inline void atomic_inc(atomic_t *v) {
    __sync_add_and_fetch(&v->counter, 1);
}

static inline int atomic_dec_and_test(atomic_t *v) {
    return __sync_sub_and_fetch(&v->counter, 1) == 0;
}

static inline void atomic_add(int i, atomic_t *v) {
    __sync_add_and_fetch(&v->counter, i);
}

static inline void atomic_sub(int i, atomic_t *v) {
    __sync_sub_and_fetch(&v->counter, i);
}

typedef struct page {
    atomic_t _refcount;
    uint64_t flags;

    union {
        struct {
            struct page *next;
            struct page *prev;
        } lru;
        struct {
            void *private;
        };
    };

    unsigned char order;
    uint32_t      compound_nr;

    // 新增：所属的 zone
    unsigned char zone_id;

    uint32_t magic;
} page_t;

#define PAGE_MAGIC 0xDEADBEEF

enum zone_stat_item {
    NR_FREE_PAGES,  // 空闲页数
    NR_ALLOC_PAGES, // 已分配页数
    NR_ACTIVE,      // 活跃页数
    NR_INACTIVE,    // 非活跃页数
    NR_ZONE_STATS
};

typedef struct zone_stats {
    atomic_t count[NR_ZONE_STATS];
} zone_stats_t;

typedef struct free_area {
    struct page *free_list;
    uint64_t     nr_free;
} free_area_t;

typedef struct per_cpu_pages {
    struct page *pages[PCPU_CACHE_HIGH];
    int          count;
    int          low;   // 低水位
    int          high;  // 高水位
    int          batch; // 批量操作数量

    uint64_t alloc_hits;
    uint64_t alloc_misses;
    uint64_t free_hits;
    uint64_t free_misses;
} per_cpu_pages_t;

typedef struct zone {
    // Buddy 分配器
    free_area_t free_area[MAX_ORDER];

    // Zone 范围
    uint64_t zone_start_pfn;
    uint64_t zone_end_pfn;
    uint64_t spanned_pages; // 跨越的页数（包括空洞）
    uint64_t present_pages; // 物理存在的页数
    uint64_t managed_pages; // 可管理的页数

    // Zone 类型
    enum zone_type type;
    const char    *name;

    // 统计信息
    zone_stats_t vm_stat;

    // Per-CPU 页面缓存（每个CPU一个）
    per_cpu_pages_t *per_cpu_pageset;

    // 锁
    spin_t lock;

    // 链表节点（用于遍历所有zone）
    struct zone *next;
} zone_t;

typedef struct zonelist {
    zone_t *zones[__MAX_NR_ZONES]; // 按优先级排序的 zone 列表
    int     nr_zones;              // zone 数量
} zonelist_t;

extern page_t  *mem_map;
extern uint64_t max_pfn;
extern uint64_t min_pfn;
extern zone_t  *zones[__MAX_NR_ZONES];
extern int      nr_zones;
extern int      nr_cpu;

// 获取页面所属的 zone
#define page_zone(page) (zones[(page)->zone_id])

static inline void set_bit(int nr, volatile uint64_t *addr) {
    *addr |= (1UL << nr);
}

static inline void clear_bit(int nr, volatile uint64_t *addr) {
    *addr &= ~(1UL << nr);
}

static inline int test_bit(int nr, const volatile uint64_t *addr) {
    return (*addr >> nr) & 1;
}

// 引用计数操作
static inline int page_ref_count(page_t *page) {
    return atomic_read(&page->_refcount);
}

static inline void set_page_refcounted(page_t *page) {
    atomic_set(&page->_refcount, 1);
}

static inline void get_page(page_t *page) {
    atomic_inc(&page->_refcount);
}

static inline bool put_page_testzero(page_t *page) {
    return atomic_dec_and_test(&page->_refcount);
}

// 复合页操作
static inline void set_compound_order(page_t *page, uint32_t order) {
    page->order       = order;
    page->compound_nr = 1U << order;
}

static inline uint32_t compound_order(page_t *page) {
    if (!PageHead(page)) return 0;
    return page->order;
}

// Zone 统计操作
static inline void zone_page_state_add(long delta, zone_t *zone, enum zone_stat_item item) {
    atomic_add(delta, &zone->vm_stat.count[item]);
}

static inline uint64_t zone_page_state(zone_t *zone, enum zone_stat_item item) {
    return atomic_read(&zone->vm_stat.count[item]);
}

// 初始化
void zones_init(uint64_t memory_size);
void add_memory_region(uintptr_t start, uintptr_t end, enum zone_type type);
void percpu_pagecache_init();

// 分配（多 zone 版本）
page_t *alloc_pages(uint32_t gfp_flags, uint32_t order);
#define alloc_page(gfp) alloc_pages(gfp, 0)

// 兼容接口（默认从 NORMAL zone）
#define __alloc_pages(order) alloc_pages(GFP_KERNEL, order)
#define __alloc_page()       alloc_pages(GFP_KERNEL, 0)

// 释放
void __free_pages(page_t *page, uint32_t order);
#define free_page(page) __free_pages(page, 0)

// Zone 查询
zone_t *get_zone(enum zone_type type);
bool    zone_has_memory(zone_t *zone);

// Zonelist 构建
void build_zonelist(zonelist_t *zl, uint32_t gfp_flags);

void init_frame_buddy(uint64_t memory_size);

uintptr_t buddy_alloc_frames(size_t count);
void buddy_free_frames(uintptr_t addr, size_t count);
