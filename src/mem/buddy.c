/**
 * NeoAetherOS per-cpu cache buddy alloc.
 * Copyright @2025-2026 by lihanrui2913.
 */
#include "mem/buddy.h"
#include "boot.h"
#include "krlibc.h"
#include "mem/bitmap.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/smp.h"
#include "term/klog.h"

const char *zone_names[__MAX_NR_ZONES] = {
#if defined(__x86_64__)
    "DMA",
#endif
    "DMA32",
    "Normal"
};

page_t *mem_map               = NULL;
uint64_t max_pfn              = 0;
uint64_t min_pfn              = 0;
zone_t *zones[__MAX_NR_ZONES] = { NULL };
int nr_zones                  = 0;
static size_t total_frames    = 0;
Bitmap usable_regions;
spin_t frame_op_lock               = SPIN_INIT;
static size_t early_last_alloc_pos = 0;

bool percpu_pagecache_initialized = false;

static inline per_cpu_pages_t *zone_pcp(zone_t *zone, int cpu) {
    return &zone->per_cpu_pageset[cpu];
}

static inline per_cpu_pages_t *this_cpu_zone_pcp(zone_t *zone) {
    return zone_pcp(zone, arch_current_cpu()->id);
}

// 根据物理地址确定所属 zone
enum zone_type pfn_to_zone_type(uint64_t pfn) {
    uint64_t phys = pfn * PAGE_SIZE;

#if defined(__x86_64__)
    if (phys < ZONE_DMA_END)
        return ZONE_DMA;
    else
#endif
        if (phys < ZONE_DMA32_END)
        return ZONE_DMA32;
    else
        return ZONE_NORMAL;
}

// 获取指定类型的 zone
zone_t *get_zone(enum zone_type type) {
    if (type >= __MAX_NR_ZONES)
        return NULL;
    return zones[type];
}

// 检查 zone 是否有内存
bool zone_has_memory(zone_t *zone) {
    return zone && zone->managed_pages > 0;
}

// GFP 标志到首选 zone 的映射
static enum zone_type gfp_zone(uint32_t gfp_flags) {
#ifdef __x86_64__
    if (gfp_flags & GFP_DMA)
        return ZONE_DMA;
#endif
    if (gfp_flags & GFP_DMA32) {
        return ZONE_DMA32;
    }
    return ZONE_NORMAL;
}

// 构建 zone fallback 列表
void build_zonelist(zonelist_t *zl, uint32_t gfp_flags) {
    enum zone_type start_zone = gfp_zone(gfp_flags);
    int idx                   = 0;

    // 从首选 zone 开始，向低端 zone fallback
    for (int i = start_zone; i >= 0; i--) {
        zone_t *zone = zones[i];
        if (zone_has_memory(zone)) {
            zl->zones[idx++] = zone;
        }
    }

    zl->nr_zones = idx;
}

static inline uint64_t __find_buddy_pfn(uint64_t pfn, uint32_t order) {
    return pfn ^ (1UL << order);
}

static inline page_t *find_buddy_page(page_t *page, uint32_t order) {
    uint64_t pfn       = page_to_pfn(page);
    uint64_t buddy_pfn = __find_buddy_pfn(pfn, order);

    if (buddy_pfn < min_pfn || buddy_pfn >= max_pfn)
        return NULL;

    page_t *buddy = pfn_to_page(buddy_pfn);

    // 伙伴必须在同一个 zone 中
    if (buddy->zone_id != page->zone_id)
        return NULL;

    return buddy;
}

static inline bool page_is_buddy(page_t *page, page_t *buddy, uint32_t order) {
    if (!PageBuddy(buddy))
        return false;
    if (buddy->order != order)
        return false;
    if (buddy->zone_id != page->zone_id)
        return false;
    return true;
}

static inline void del_page_from_free_list(page_t *page, zone_t *zone, uint32_t order) {
    if (page->lru.prev) {
        page->lru.prev->lru.next = page->lru.next;
    } else {
        zone->free_area[order].free_list = page->lru.next;
    }

    if (page->lru.next) {
        page->lru.next->lru.prev = page->lru.prev;
    }

    page->lru.next = NULL;
    page->lru.prev = NULL;
    zone->free_area[order].nr_free--;
}

static inline void add_to_free_list(page_t *page, zone_t *zone, uint32_t order) {
    page->lru.next = zone->free_area[order].free_list;
    page->lru.prev = NULL;

    if (zone->free_area[order].free_list) {
        zone->free_area[order].free_list->lru.prev = page;
    }

    zone->free_area[order].free_list = page;
    zone->free_area[order].nr_free++;

    page->order = order;
    SetPageBuddy(page);
}

static inline void prep_new_page(page_t *page, uint32_t order) {
    page->flags = 0;
    set_page_refcounted(page);

    if (order > 0) {
        SetPageHead(page);
        set_compound_order(page, order);

        for (uint32_t i = 1; i < (1U << order); i++) {
            page_t *p = page + i;
            p->flags  = 0;
            SetPageCompound(p);
            atomic_set(&p->_refcount, 0);
            p->zone_id = page->zone_id; // 继承 zone_id
        }
    }

    page->magic = PAGE_MAGIC;
}

static page_t *expand(zone_t *zone, page_t *page, uint32_t low_order, uint32_t high_order) {
    uint64_t size = 1UL << high_order;

    while (high_order > low_order) {
        high_order--;
        size >>= 1;

        page_t *buddy = page + size;
        add_to_free_list(buddy, zone, high_order);
    }

    return page;
}

static page_t *__rmqueue_smallest(zone_t *zone, uint32_t order) {
    uint32_t current_order;

    for (current_order = order; current_order < MAX_ORDER; current_order++) {
        free_area_t *area = &zone->free_area[current_order];
        page_t *page      = area->free_list;

        if (!page)
            continue;

        del_page_from_free_list(page, zone, current_order);
        ClearPageBuddy(page);

        if (current_order > order) {
            expand(zone, page, order, current_order);
        }

        // 更新统计
        zone_page_state_add(-(1 << order), zone, NR_FREE_PAGES);
        zone_page_state_add(1 << order, zone, NR_ALLOC_PAGES);

        return page;
    }

    return NULL;
}

// Per-CPU 缓存分配
static page_t *rmqueue_pcplist(zone_t *zone) {
    per_cpu_pages_t *pcp = this_cpu_zone_pcp(zone);

    // 快速路径：从缓存获取
    if (pcp->count > 0) {
        pcp->count--;
        pcp->alloc_hits++;
        page_t *page = pcp->pages[pcp->count];

        // 更新统计
        zone_page_state_add(-1, zone, NR_FREE_PAGES);
        zone_page_state_add(1, zone, NR_ALLOC_PAGES);

        return page;
    }

    pcp->alloc_misses++;

    // 慢速路径：批量补充
    spin_lock(zone->lock);

    int target = MIN(pcp->batch, pcp->high - pcp->count);
    for (int i = 0; i < target; i++) {
        page_t *page = __rmqueue_smallest(zone, 0);
        if (!page)
            break;

        pcp->pages[pcp->count++] = page;

        // __rmqueue_smallest 已更新统计，需要回调
        zone_page_state_add(1, zone, NR_FREE_PAGES);
        zone_page_state_add(-1, zone, NR_ALLOC_PAGES);
    }

    spin_unlock(zone->lock);

    // 再次尝试分配
    if (pcp->count > 0) {
        pcp->count--;
        zone_page_state_add(-1, zone, NR_FREE_PAGES);
        zone_page_state_add(1, zone, NR_ALLOC_PAGES);
        return pcp->pages[pcp->count];
    }

    return NULL;
}

// 从单个 zone 分配
static page_t *rmqueue(zone_t *zone, uint32_t order, uint32_t gfp_flags) {
    page_t *page;

    if (!zone_has_memory(zone))
        return NULL;

    // 单页分配：优先 per-CPU 缓存
    if (percpu_pagecache_initialized) {
        if (order == 0) {
            page = rmqueue_pcplist(zone);
            if (page)
                return page;
        }
    }

    // 多页或缓存失败：从 buddy
    spin_lock(zone->lock);
    page = __rmqueue_smallest(zone, order);
    spin_unlock(zone->lock);

    return page;
}

page_t *alloc_pages(uint32_t gfp_flags, uint32_t order) {
    page_t *page = NULL;
    zonelist_t zl;

    if (order >= MAX_ORDER)
        return NULL;

    // 构建 zonelist
    build_zonelist(&zl, gfp_flags);

    // 按优先级尝试从各个 zone 分配
    for (int i = 0; i < zl.nr_zones; i++) {
        zone_t *zone = zl.zones[i];
        page         = rmqueue(zone, order, gfp_flags);
        if (page) {
            prep_new_page(page, order);
            return page;
        }
    }

    // 所有 zone 都失败
    if (!(gfp_flags & GFP_NOWAIT)) {
        // TODO: 触发内存回收
    }

    return NULL;
}

static inline uint64_t __free_one_page(page_t *page, uint64_t pfn, zone_t *zone, uint32_t order) {
    uint64_t combined_pfn;
    page_t *buddy;

    while (order < MAX_ORDER - 1) {
        buddy = find_buddy_page(page, order);

        if (!buddy || !page_is_buddy(page, buddy, order))
            break;

        del_page_from_free_list(buddy, zone, order);
        ClearPageBuddy(buddy);

        combined_pfn = pfn & ~(1UL << order);
        page         = pfn_to_page(combined_pfn);
        pfn          = combined_pfn;
        order++;
    }

    add_to_free_list(page, zone, order);

    return pfn;
}

static void free_pcppages_bulk(zone_t *zone, per_cpu_pages_t *pcp, int count) {
    spin_lock(zone->lock);

    while (count > 0 && pcp->count > 0) {
        pcp->count--;
        count--;

        page_t *page = pcp->pages[pcp->count];
        uint64_t pfn = page_to_pfn(page);

        __free_one_page(page, pfn, zone, 0);

        // 更新统计
        zone_page_state_add(1, zone, NR_FREE_PAGES);
        zone_page_state_add(-1, zone, NR_ALLOC_PAGES);
    }

    spin_unlock(zone->lock);
}

void __free_pages(page_t *page, uint32_t order) {
    if (!page || page->magic != PAGE_MAGIC)
        return;

    if (!put_page_testzero(page))
        return;

    zone_t *zone = page_zone(page);
    if (!zone)
        return;

    // 清除复合页标记
    if (order > 0) {
        ClearPageHead(page);
        for (uint32_t i = 1; i < (1U << order); i++) {
            ClearPageCompound(page + i);
        }
    }

    // 单页：优先放入 per-CPU 缓存
    if (percpu_pagecache_initialized) {
        if (order == 0) {
            per_cpu_pages_t *pcp = this_cpu_zone_pcp(zone);

            if (pcp->count < pcp->high) {
                pcp->pages[pcp->count++] = page;
                pcp->free_hits++;

                zone_page_state_add(1, zone, NR_FREE_PAGES);
                zone_page_state_add(-1, zone, NR_ALLOC_PAGES);
                return;
            }

            pcp->free_misses++;

            // 缓存满，批量释放
            free_pcppages_bulk(zone, pcp, pcp->batch);

            if (pcp->count < pcp->high) {
                pcp->pages[pcp->count++] = page;
                zone_page_state_add(1, zone, NR_FREE_PAGES);
                zone_page_state_add(-1, zone, NR_ALLOC_PAGES);
                return;
            }
        }
    }

    // 多页：直接归还 buddy
    uint64_t pfn = page_to_pfn(page);

    spin_lock(zone->lock);
    __free_one_page(page, pfn, zone, order);
    zone_page_state_add(1 << order, zone, NR_FREE_PAGES);
    zone_page_state_add(-(1 << order), zone, NR_ALLOC_PAGES);
    spin_unlock(zone->lock);
}

// 初始化单个 zone
static void init_zone(zone_t *zone, enum zone_type type, uint64_t start_pfn, uint64_t end_pfn) {
    memset(zone, 0, sizeof(zone_t));

    zone->type           = type;
    zone->name           = zone_names[type];
    zone->zone_start_pfn = start_pfn;
    zone->zone_end_pfn   = end_pfn;
    zone->spanned_pages  = end_pfn - start_pfn;
    zone->present_pages  = 0; // 稍后添加内存时更新
    zone->managed_pages  = 0;

    zone->lock = SPIN_INIT;

    // 初始化统计
    for (int i = 0; i < NR_ZONE_STATS; i++) {
        atomic_set(&zone->vm_stat.count[i], 0);
    }
}

uint64_t alloc_frames_early(size_t count) {
    spin_lock(frame_op_lock);
    Bitmap *bitmap     = &usable_regions;
    size_t frame_index = bitmap_find_range_from(bitmap, count, true, early_last_alloc_pos);
    bitmap_set_range(bitmap, frame_index, frame_index + count, false);
    early_last_alloc_pos = frame_index + count - 1;
    spin_unlock(frame_op_lock);
    return frame_index * PAGE_SIZE;
}

void *early_alloc(size_t size) {
    return (void *)phys_to_virt(alloc_frames_early((size + PAGE_SIZE - 1) / PAGE_SIZE));
}

// 全局初始化
void zones_init(uint64_t memory_size) {
    min_pfn = 0;
    max_pfn = memory_size / PAGE_SIZE;

    uint64_t total_pages = max_pfn - min_pfn;

    // 分配 mem_map
    size_t mem_map_size = total_pages * sizeof(page_t);
    mem_map             = (page_t *)early_alloc(mem_map_size);
    memset(mem_map, 0, mem_map_size);

    // 初始化所有页
    for (uint64_t i = 0; i < total_pages; i++) {
        page_t *page = &mem_map[i];
        atomic_set(&page->_refcount, 1);
        SetPageReserved(page);
        page->magic = PAGE_MAGIC;

        // 设置 zone_id
        uint64_t pfn  = min_pfn + i;
        page->zone_id = pfn_to_zone_type(pfn);
    }

    // 为每个可能的 zone 类型分配结构
    for (int i = 0; i < __MAX_NR_ZONES; i++) {
        zones[i] = (zone_t *)early_alloc(sizeof(zone_t));
    }

    // 初始化 zone（根据系统内存范围）
    uint64_t dma_end   = MIN(max_pfn, ZONE_DMA_END / PAGE_SIZE);
    uint64_t dma32_end = MIN(max_pfn, ZONE_DMA32_END / PAGE_SIZE);

#if defined(__x86_64__)
    if (min_pfn < dma_end) {
        init_zone(zones[ZONE_DMA], ZONE_DMA, min_pfn, dma_end);
        nr_zones++;
    }
#endif

    if (dma_end < dma32_end) {
        init_zone(zones[ZONE_DMA32], ZONE_DMA32, dma_end, dma32_end);
        nr_zones++;
    }

    init_zone(zones[ZONE_NORMAL], ZONE_NORMAL, dma32_end, memory_size);
    nr_zones++;
}

// 添加内存区域到指定 zone
void add_memory_region(uintptr_t start, uintptr_t end, enum zone_type type) {
    zone_t *zone = zones[type];
    if (!zone) {
        return;
    }

    uint64_t start_pfn = start / PAGE_SIZE;
    uint64_t end_pfn   = end / PAGE_SIZE;

    spin_lock(zone->lock);

    for (uint64_t pfn = start_pfn; pfn < end_pfn;) {
        page_t *page = pfn_to_page(pfn);

        if (PageReserved(page)) {
            ClearPageReserved(page);
            atomic_set(&page->_refcount, 0);
        }

        // 确保 zone_id 正确
        page->zone_id = type;

        // 找到最大对齐块
        uint32_t order = 0;
        uint64_t size  = 1;

        while (order < MAX_ORDER - 1) {
            if ((pfn & ((1UL << (order + 1)) - 1)) != 0)
                break;
            if (pfn + (size << 1) > end_pfn)
                break;

            order++;
            size <<= 1;
        }

        // 添加到 buddy
        __free_one_page(page, pfn, zone, order);

        zone->managed_pages += size;
        zone->present_pages += size;
        zone_page_state_add(size, zone, NR_FREE_PAGES);

        pfn += size;
    }

    spin_unlock(zone->lock);
}

// Per-CPU 缓存初始化
void percpu_pagecache_init() {
    for (int i = 0; i < __MAX_NR_ZONES; i++) {
        zone_t *zone = zones[i];
        if (!zone_has_memory(zone))
            continue;

        size_t total_size     = sizeof(per_cpu_pages_t) * get_cpu_count();
        size_t page_size      = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
        uint64_t pset_phy     = alloc_frames(page_size);
        zone->per_cpu_pageset = (per_cpu_pages_t *)driver_phys_to_virt(pset_phy);
        page_map_range(
            get_kernel_pagedir(),
            (uint64_t)zone->per_cpu_pageset,
            pset_phy,
            total_size,
            KERNEL_PTE_FLAGS
        );

        for (int cpu = 0; cpu < get_cpu_count(); cpu++) {
            per_cpu_pages_t *pcp = zone_pcp(zone, cpu);
            memset(pcp, 0, sizeof(per_cpu_pages_t));

            pcp->low   = PCPU_CACHE_LOW;
            pcp->high  = PCPU_CACHE_HIGH;
            pcp->batch = PCPU_BATCH;
        }
    }

    percpu_pagecache_initialized = true;
}

// 清空所有缓存
void drain_all_pages(void) {
    for (int i = 0; i < __MAX_NR_ZONES; i++) {
        zone_t *zone = zones[i];
        if (!zone_has_memory(zone)) {
            continue;
        }

        for (int cpu = 0; cpu < MAX_CPU; cpu++) {
            per_cpu_pages_t *pcp = zone_pcp(zone, cpu);
            if (pcp->count > 0) {
                free_pcppages_bulk(zone, pcp, pcp->count);
            }
        }
    }
}

static uintptr_t get_zone_boundary(enum zone_type type) {
    switch (type) {
#if defined(__x86_64__)
    case ZONE_DMA:
        return ZONE_DMA_END;
#endif
    case ZONE_DMA32:
        return ZONE_DMA32_END;
    case ZONE_NORMAL:
        return UINTPTR_MAX;
    default:
        return 0;
    }
}

static void process_memory_region(uintptr_t start, uintptr_t end) {
    // 对齐
    start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    end   = end & ~(PAGE_SIZE - 1);

    if (start >= end)
        return;

    // 检查是否在 bitmap 中标记为可用
    size_t start_frame = start / PAGE_SIZE;
    size_t end_frame   = end / PAGE_SIZE;

    uintptr_t current = start;

    while (current < end) {
        // 确定当前位置所属的 zone
        enum zone_type zone_type = pfn_to_zone_type(current / PAGE_SIZE);

        // 找到同一 zone 的连续区域
        uintptr_t zone_end = get_zone_boundary(zone_type);
        if (zone_end > end)
            zone_end = end;

        // 检查这段区域是否在 bitmap 中可用
        uint64_t last_non_usable_addr = current;
        for (size_t frame = current / PAGE_SIZE; frame < zone_end / PAGE_SIZE; frame++) {
            if (!bitmap_get(&usable_regions, frame)) {
                last_non_usable_addr = (frame + 1) * PAGE_SIZE;
            }
        }

        if (zone_end > last_non_usable_addr) {
            add_memory_region(last_non_usable_addr, zone_end, zone_type);
        }

        current = zone_end;
    }
}

void init_frame_buddy(uint64_t memory_size) {
    boot_memory_map_t *memory_map = boot_get_memory_map();

    if (!memory_map)
        return;

    total_frames = memory_size / PAGE_SIZE;
    if (total_frames == 0)
        return;

    size_t bitmap_size      = (memory_size / PAGE_SIZE + 7) / 8;
    uint64_t bitmap_address = 0;

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct boot_memory_map_entry *region = &memory_map->entries[i];

#if defined(__x86_64__)
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type == BOOT_MMAP_USABLE) {
            if (region->length >= bitmap_size) {
                bitmap_address = region->base;
                break;
            }
        }
    }

    bitmap_init(&usable_regions, (uint8_t *)phys_to_virt(bitmap_address), bitmap_size);

    size_t origin_frames = 0;
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct boot_memory_map_entry *region = &memory_map->entries[i];

        size_t start_frame = region->base / PAGE_SIZE;
        size_t frame_count = region->length / PAGE_SIZE;

#if defined(__x86_64__)
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type == BOOT_MMAP_USABLE) {
            origin_frames += frame_count;
            bitmap_set_range(&usable_regions, start_frame, start_frame + frame_count, true);
        }
    }

#if defined(__x86_64__)
    size_t low_1M_frame_count = 0x100000 / PAGE_SIZE;
    bitmap_set_range(&usable_regions, 0, low_1M_frame_count, false);
#endif

    size_t bitmap_frame_start = bitmap_address / PAGE_SIZE;
    size_t bitmap_frame_end   = (bitmap_address + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    bitmap_set_range(&usable_regions, bitmap_frame_start, bitmap_frame_end, false);

    zones_init(memory_size);
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct boot_memory_map_entry *region = &memory_map->entries[i];

#if defined(__x86_64__)
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type != BOOT_MMAP_USABLE)
            continue;

        uint64_t addr = region->base;
        uint64_t len  = region->length;

        if (addr == bitmap_address) {
            addr += (bitmap_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            len -= (bitmap_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        }

        process_memory_region(addr, addr + len);
    }

    frame_allocator.origin_frames = origin_frames;
    frame_allocator.usable_frames = origin_frames;
    frame_allocator.total_frames  = total_frames;
    logkf(
        "buddy: total frames = %zu, usable_frames = %zu\n",
        total_frames,
        frame_allocator.usable_frames
    );
}

static size_t next_power_of_2(size_t n) {
    if (n == 0)
        return 1;
    if ((n & (n - 1)) == 0)
        return n; // 已经是2的幂次

    size_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

static size_t log2_floor(size_t n) {
    size_t log = 0;
    while (n > 1) {
        n >>= 1;
        log++;
    }
    return log;
}

// 分配页框
uintptr_t buddy_alloc_frames(size_t count) {
    size_t required_pages = next_power_of_2(count);
    size_t order          = log2_floor(required_pages);

    page_t *page = alloc_pages(GFP_KERNEL_NORMAL, order);
    if (unlikely(page == NULL))
        return 0;
    uint64_t idx = page - mem_map;

    return idx * PAGE_SIZE;
}

// 释放页框
void buddy_free_frames(uintptr_t addr, size_t count) {
    if (!addr)
        return;

    size_t required_pages = next_power_of_2(count);
    size_t order          = log2_floor(required_pages);

    uint64_t idx = addr / PAGE_SIZE;
    if (bitmap_get(&usable_regions, idx) == false)
        return;

    page_t *page = &mem_map[idx];

    __free_pages(page, order);
}
