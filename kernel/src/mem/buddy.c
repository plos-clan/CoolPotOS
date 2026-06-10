/**
 * NeoAetherOS per-cpu cache buddy alloc.
 * Copyright @2025-2026 by lihanrui2913.
 */
#include "mem/buddy.h"
#include "krlibc.h"
#include "mem/bitmap.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/page_ref.h"

Bitmap using_regions;

const char *zone_names[__MAX_NR_ZONES] = {
#if defined(__x86_64__)
    "DMA",
#endif
    "DMA32",
    "Normal"
};

zone_t *zones[__MAX_NR_ZONES] = { NULL };
int nr_zones                  = 0;

extern void *early_alloc(size_t size);

typedef struct page_list {
    size_t entry_num;
    uintptr_t next_page;
} page_list_t;

#define PAGE_LIST_ENTRY_CAPACITY ((PAGE_SIZE - sizeof(page_list_t)) / sizeof(uintptr_t))

static uintptr_t metadata_pool_phys = 0;
static size_t metadata_pool_pages   = 0;
static size_t metadata_pool_used    = 0;
static uintptr_t metadata_free_list = 0;
static spin_t metadata_lock         = SPIN_INIT;

static inline size_t order_to_index(size_t order) {
    return order - MIN_ORDER;
}

static inline uint64_t order_to_pages(size_t order) {
    return 1ULL << (order - MIN_ORDER);
}

static inline uint64_t order_to_bytes(size_t order) {
    return 1ULL << order;
}

static inline bool order_valid(size_t order) {
    return order >= MIN_ORDER && order < MAX_ORDER;
}

static inline uintptr_t zone_phys_start(zone_t *zone) {
    return zone->zone_start_pfn * PAGE_SIZE;
}

static inline uintptr_t zone_phys_end(zone_t *zone) {
    return zone->zone_end_pfn * PAGE_SIZE;
}

static inline page_list_t *page_list_virt(uintptr_t phys) {
    return phys ? (page_list_t *)phys_to_virt(phys) : NULL;
}

static inline uintptr_t *page_list_entries(uintptr_t phys) {
    return (uintptr_t *)((uint8_t *)phys_to_virt(phys) + sizeof(page_list_t));
}

static uintptr_t metadata_page_alloc(void) {
    uintptr_t phys = 0;

    spin_lock(metadata_lock);

    if (metadata_free_list != 0) {
        phys               = metadata_free_list;
        metadata_free_list = page_list_virt(phys)->next_page;
    } else {
        asserts(
            metadata_pool_used < metadata_pool_pages,
            "metadata_page_alloc: metadata_pool_used < metadata_pool_pages."
        );
        phys = metadata_pool_phys + metadata_pool_used * PAGE_SIZE;
        metadata_pool_used++;
    }

    spin_unlock(metadata_lock);

    memset(phys_to_virt(phys), 0, PAGE_SIZE);
    return phys;
}

static void metadata_page_free(uintptr_t phys) {
    page_list_t *list = page_list_virt(phys);
    asserts(list != NULL, "metadata_page_free: list is null.");

    memset(list, 0, PAGE_SIZE);

    spin_lock(metadata_lock);
    list->next_page    = metadata_free_list;
    metadata_free_list = phys;
    spin_unlock(metadata_lock);
}

static bool count_to_order(size_t count, size_t *order_out, size_t *pages_out) {
    if (!order_out || !pages_out || count == 0)
        return false;

    size_t pages     = 1;
    size_t max_pages = (size_t)order_to_pages(MAX_ORDER - 1);

    while (pages < count) {
        if (pages > (SIZE_MAX >> 1))
            return false;
        pages <<= 1;
    }

    if (pages > max_pages)
        return false;

    size_t order = MIN_ORDER;
    size_t tmp   = pages;
    while (tmp > 1) {
        tmp >>= 1;
        order++;
    }

    if (!order_valid(order))
        return false;

    *order_out = order;
    *pages_out = pages;
    return true;
}

static bool count_to_pages(size_t count, size_t *pages_out) {
    size_t ignored_order = 0;
    return count_to_order(count, &ignored_order, pages_out);
}

static bool zone_block_valid(zone_t *zone, uintptr_t addr, size_t order) {
    if (!zone || !order_valid(order))
        return false;

    uint64_t block_bytes = order_to_bytes(order);
    uintptr_t start      = zone_phys_start(zone);
    uintptr_t end        = zone_phys_end(zone);
    uintptr_t block_end  = addr + block_bytes;

    if (block_end < addr)
        return false;
    if ((addr & (block_bytes - 1)) != 0)
        return false;
    if (addr < start || block_end > end)
        return false;
    return true;
}

static void page_list_push(zone_t *zone, size_t order, uintptr_t block_phys) {
    size_t index        = order_to_index(order);
    uintptr_t head_phys = zone->allocator.free_area[index];
    uintptr_t phys      = head_phys;
    uintptr_t prev_phys = 0;
    page_list_t *list   = NULL;

    while (phys) {
        list = page_list_virt(phys);
        if (list->entry_num < PAGE_LIST_ENTRY_CAPACITY)
            break;

        uintptr_t next_phys = list->next_page;
        if (prev_phys != 0 && list->entry_num == 0) {
            page_list_virt(prev_phys)->next_page = next_phys;
            metadata_page_free(phys);
            phys = next_phys;
            continue;
        }

        prev_phys = phys;
        phys      = next_phys;
    }

    if (!phys) {
        uintptr_t new_head_phys          = metadata_page_alloc();
        page_list_t *new_head            = page_list_virt(new_head_phys);
        new_head->entry_num              = 0;
        new_head->next_page              = head_phys;
        zone->allocator.free_area[index] = new_head_phys;
        list                             = new_head;
        phys                             = new_head_phys;
    }

    uintptr_t *entries         = page_list_entries(phys);
    entries[list->entry_num++] = block_phys;
}

static uintptr_t page_list_pop(zone_t *zone, size_t order) {
    size_t index        = order_to_index(order);
    uintptr_t head_phys = zone->allocator.free_area[index];
    uintptr_t phys      = head_phys;
    uintptr_t prev_phys = 0;

    while (phys) {
        page_list_t *list   = page_list_virt(phys);
        uintptr_t next_phys = list->next_page;

        if (list->entry_num != 0) {
            uintptr_t *entries           = page_list_entries(phys);
            uintptr_t block              = entries[list->entry_num - 1];
            entries[list->entry_num - 1] = 0;
            list->entry_num--;

            if (list->entry_num == 0 && prev_phys != 0) {
                page_list_virt(prev_phys)->next_page = next_phys;
                metadata_page_free(phys);
            }
            return block;
        }

        if (prev_phys != 0) {
            page_list_virt(prev_phys)->next_page = next_phys;
            metadata_page_free(phys);
            phys = next_phys;
            continue;
        }

        prev_phys = phys;
        phys      = next_phys;
    }

    return 0;
}

static bool page_list_take(zone_t *zone, size_t order, uintptr_t target_phys) {
    size_t index        = order_to_index(order);
    uintptr_t phys      = zone->allocator.free_area[index];
    uintptr_t prev_phys = 0;

    while (phys) {
        page_list_t *list   = page_list_virt(phys);
        uintptr_t *entries  = page_list_entries(phys);
        uintptr_t next_phys = list->next_page;

        for (size_t i = 0; i < list->entry_num; i++) {
            if (entries[i] != target_phys)
                continue;

            entries[i]                   = entries[list->entry_num - 1];
            entries[list->entry_num - 1] = 0;
            list->entry_num--;

            if (list->entry_num == 0 && prev_phys != 0) {
                page_list_virt(prev_phys)->next_page = next_phys;
                metadata_page_free(phys);
            }
            return true;
        }

        if (prev_phys != 0 && list->entry_num == 0) {
            page_list_virt(prev_phys)->next_page = next_phys;
            metadata_page_free(phys);
            phys = next_phys;
            continue;
        }

        prev_phys = phys;
        phys      = next_phys;
    }

    return false;
}

static uintptr_t buddy_alloc_order_locked(zone_t *zone, size_t target_order) {
    size_t source_order  = target_order;
    uintptr_t block_phys = 0;

    while (source_order < MAX_ORDER) {
        block_phys = page_list_pop(zone, source_order);
        if (block_phys != 0) {
            break;
        }
        source_order++;
    }

    if (block_phys == 0) {
        return 0;
    }

    while (source_order > target_order) {
        source_order--;
        const uintptr_t buddy_phys = block_phys + order_to_bytes(source_order);
        page_list_push(zone, source_order, buddy_phys);
    }

    return block_phys;
}

static void buddy_free_zone_locked(zone_t *zone, uintptr_t addr, size_t order) {
    size_t base_order = order;

    while (order < MAX_ORDER - 1) {
        const uintptr_t buddy_phys = addr ^ order_to_bytes(order);
        if (!zone_block_valid(zone, buddy_phys, order)) {
            break;
        }
        if (!page_list_take(zone, order, buddy_phys)) {
            break;
        }

        if (buddy_phys < addr) {
            addr = buddy_phys;
        }
        order++;
    }

    page_list_push(zone, order, addr);
    zone->free_pages += order_to_pages(base_order);
}

enum zone_type phys_to_zone_type(uintptr_t phys) {
#if defined(__x86_64__)
    if (phys < ZONE_DMA_END)
        return ZONE_DMA;
#endif
    if (phys < ZONE_DMA32_END)
        return ZONE_DMA32;
    return ZONE_NORMAL;
}

zone_t *get_zone(enum zone_type type) {
    if (type >= __MAX_NR_ZONES) {
        return NULL;
    }
    return zones[type];
}

bool zone_has_memory(zone_t *zone) {
    return zone && zone->free_pages > 0;
}

void buddy_free_zone(zone_t *zone, uintptr_t addr, size_t order) {
    if (!zone_block_valid(zone, addr, order)) {
        return;
    }

    spin_lock(zone->allocator.lock);
    buddy_free_zone_locked(zone, addr, order);
    spin_unlock(zone->allocator.lock);
}

uintptr_t buddy_alloc_zone(zone_t *zone, size_t count) {
    if (!zone || count == 0) {
        return 0;
    }

    size_t order          = 0;
    size_t required_pages = 0;
    if (!count_to_order(count, &order, &required_pages)) {
        return 0;
    }

    bool irq = arch_check_interrupt();

    arch_close_interrupt();

    spin_lock(zone->allocator.lock);

    if (zone->free_pages < required_pages) {
        spin_unlock(zone->allocator.lock);
        if (irq)
            arch_open_interrupt();
        return 0;
    }

    uintptr_t addr = buddy_alloc_order_locked(zone, order);
    if (addr != 0)
        zone->free_pages -= required_pages;

    spin_unlock(zone->allocator.lock);
    if (irq)
        arch_open_interrupt();
    return addr;
}

static void init_zone(zone_t *zone, enum zone_type type, uint64_t start_pfn, uint64_t end_pfn) {
    memset(zone, 0, sizeof(*zone));

    zone->type           = type;
    zone->name           = zone_names[type];
    zone->zone_start_pfn = start_pfn;
    zone->zone_end_pfn   = end_pfn;
    zone->managed_pages  = 0;
    zone->free_pages     = 0;

    zone->allocator.lock = SPIN_INIT;

    for (size_t i = 0; i < ORDER_COUNT; i++) {
        uintptr_t head_phys          = metadata_page_alloc();
        zone->allocator.free_area[i] = head_phys;
    }
}

static void create_zone(enum zone_type type, uint64_t start_pfn, uint64_t end_pfn) {
    if (type >= __MAX_NR_ZONES || end_pfn <= start_pfn) {
        zones[type] = NULL;
        return;
    }

    zone_t *zone = early_alloc(sizeof(zone_t));
    asserts(zone != NULL, "create_zone: zone is null.");

    init_zone(zone, type, start_pfn, end_pfn);
    zones[type] = zone;
    nr_zones++;
}

void buddy_init(void) {
    memset(zones, 0, sizeof(zones));
    nr_zones           = 0;
    metadata_free_list = 0;
    metadata_pool_used = 0;
    metadata_lock      = SPIN_INIT;

    size_t total_frames = get_memory_size() / PAGE_SIZE;
    size_t head_pages   = ORDER_COUNT * __MAX_NR_ZONES;
    metadata_pool_pages = (total_frames + PAGE_LIST_ENTRY_CAPACITY - 1) / PAGE_LIST_ENTRY_CAPACITY;
    metadata_pool_pages += head_pages + 16;

    void *metadata_pool_virt = early_alloc(metadata_pool_pages * PAGE_SIZE);
    asserts(metadata_pool_virt != NULL, "buddy_init: metadata_pool_virt is null");
    metadata_pool_phys = virt_to_phys(metadata_pool_virt);
    metadata_pool_used = 0;

    uint64_t max_pfn       = get_memory_size() / PAGE_SIZE;
    uint64_t dma32_end_pfn = MIN(max_pfn, ZONE_DMA32_END / PAGE_SIZE);

#if defined(__x86_64__)
    uint64_t dma_end_pfn = MIN(max_pfn, ZONE_DMA_END / PAGE_SIZE);

    create_zone(ZONE_DMA, 0, dma_end_pfn);
    create_zone(ZONE_DMA32, dma_end_pfn, dma32_end_pfn);
    create_zone(ZONE_NORMAL, dma32_end_pfn, max_pfn);
#else
    create_zone(ZONE_DMA32, 0, dma32_end_pfn);
    create_zone(ZONE_NORMAL, dma32_end_pfn, max_pfn);
#endif

    size_t bitmap_bytes = ((max_pfn + 7) / 8);
    if (bitmap_bytes == 0)
        bitmap_bytes = 1;
    void *bitmap_buffer = early_alloc(bitmap_bytes);
    asserts(bitmap_buffer != NULL, "buddy_init: bitmap_buffer is null.");
    bitmap_init(&using_regions, bitmap_buffer, bitmap_bytes);
}

static size_t floor_order_for_size(uint64_t bytes) {
    size_t order = MIN_ORDER;
    while (order + 1 < MAX_ORDER && (1ULL << (order + 1)) <= bytes)
        order++;
    return order;
}

void add_memory_region(uintptr_t start, uintptr_t end, enum zone_type type) {
    zone_t *zone = get_zone(type);
    if (!zone || start >= end)
        return;

    start = PADDING_UP(start, PAGE_SIZE);
    end   = PADDING_DOWN(end, PAGE_SIZE);
    if (start >= end)
        return;

    uintptr_t zone_start = zone_phys_start(zone);
    uintptr_t zone_end   = zone_phys_end(zone);
    if (start < zone_start)
        start = zone_start;
    if (end > zone_end)
        end = zone_end;
    if (start >= end)
        return;

    spin_lock(zone->allocator.lock);

    uintptr_t current = start;
    while (current < end) {
        uint64_t remaining    = end - current;
        size_t order_by_size  = floor_order_for_size(remaining);
        size_t order_by_align = (current == 0)
                                    ? (MAX_ORDER - 1)
                                    : MIN((size_t)__builtin_ctzll((unsigned long long)current),
                                          (size_t)(MAX_ORDER - 1));

        size_t order = MIN(order_by_size, order_by_align);
        if (order < MIN_ORDER)
            order = MIN_ORDER;

        buddy_free_zone_locked(zone, current, order);
        zone->managed_pages += order_to_pages(order);
        current += order_to_bytes(order);
    }

    spin_unlock(zone->allocator.lock);
}

uint64_t alloc_frames(size_t count) {
    if (count == 0)
        return 0;

    size_t required_pages = 0;
    if (!count_to_pages(count, &required_pages))
        return 0;

    uintptr_t addr = 0;

    if (zones[ZONE_NORMAL] && zone_has_memory(zones[ZONE_NORMAL])) {
        addr = buddy_alloc_zone(zones[ZONE_NORMAL], count);
        if (addr != 0)
            goto out;
    }

    if (zones[ZONE_DMA32] && zone_has_memory(zones[ZONE_DMA32])) {
        addr = buddy_alloc_zone(zones[ZONE_DMA32], count);
        if (addr != 0)
            goto out;
    }

#if defined(__x86_64__)
    if (zones[ZONE_DMA] && zone_has_memory(zones[ZONE_DMA])) {
        addr = buddy_alloc_zone(zones[ZONE_DMA], count);
        if (addr != 0)
            goto out;
    }
#endif

out:
    if (addr == 0)
        return 0;

    size_t page_index = addr / PAGE_SIZE;
    bitmap_set_range(&using_regions, page_index, page_index + required_pages, true);

    for (size_t offset = 0; offset < required_pages; offset++) {
        page_t *page = get_page_ref(addr + offset * PAGE_SIZE);
        if (page)
            page_ref(page);
    }

    return addr;
}

static bool claim_last_page_refs(uintptr_t addr, size_t pages) {
    for (size_t offset = 0; offset < pages; offset++) {
        page_t *page = get_page_ref(addr + offset * PAGE_SIZE);
        if (page && page_try_release_last(page))
            continue;

        for (size_t rollback = 0; rollback < offset; rollback++) {
            page_ref(get_page_ref(addr + rollback * PAGE_SIZE));
        }
        return false;
    }

    return true;
}

static bool pages_are_unreferenced(uintptr_t addr, size_t pages) {
    for (size_t offset = 0; offset < pages; offset++) {
        page_t *page = get_page_ref(addr + offset * PAGE_SIZE);
        if (!page || page_refcount_read(page) != 0)
            return false;
    }

    return true;
}

static void free_frames_common(uintptr_t addr, size_t count, bool refs_already_released) {
    if (addr == 0 || count == 0)
        return;
    if ((addr & (PAGE_SIZE - 1)) != 0)
        return;
    if (addr > get_memory_size())
        return;

    size_t required_order = 0;
    size_t required_pages = 0;
    if (!count_to_order(count, &required_order, &required_pages))
        return;

    enum zone_type type = phys_to_zone_type(addr);
    zone_t *zone        = get_zone(type);
    if (!zone)
        return;

    uintptr_t zone_start = zone_phys_start(zone);
    uintptr_t zone_end   = zone_phys_end(zone);
    uintptr_t free_end   = addr + required_pages * PAGE_SIZE;

    if (free_end < addr || addr < zone_start || free_end > zone_end)
        return;

    size_t start_page_index = addr / PAGE_SIZE;
    if (start_page_index + required_pages < start_page_index
        || start_page_index + required_pages > using_regions.length
        || start_page_index + required_pages > get_usable_regions()->length)
        return;

    bool irq = arch_check_interrupt();

    arch_close_interrupt();

    spin_lock(zone->allocator.lock);

    for (size_t offset = 0; offset < required_pages; offset++) {
        if (!bitmap_get(&using_regions, start_page_index + offset)) {
            spin_unlock(zone->allocator.lock);
            if (irq)
                arch_open_interrupt();
            return;
        }
        if (!bitmap_get(get_usable_regions(), start_page_index + offset)) {
            spin_unlock(zone->allocator.lock);
            if (irq)
                arch_open_interrupt();
            return;
        }
    }

    if (refs_already_released) {
        if (!pages_are_unreferenced(addr, required_pages)) {
            spin_unlock(zone->allocator.lock);
            if (irq)
                arch_open_interrupt();
            return;
        }
    } else {
        if (!claim_last_page_refs(addr, required_pages)) {
            spin_unlock(zone->allocator.lock);
            if (irq)
                arch_open_interrupt();
            return;
        }
    }

    bitmap_set_range(&using_regions, start_page_index, start_page_index + required_pages, false);
    buddy_free_zone_locked(zone, addr, required_order);

    spin_unlock(zone->allocator.lock);

    if (irq)
        arch_open_interrupt();
}

void free_frames(uintptr_t addr, size_t count) {
    free_frames_common(addr, count, false);
}

void free_frame(uint64_t addr) {
    free_frames_common(addr, 1, false);
}

void free_frames_released(uintptr_t addr, size_t count) {
    free_frames_common(addr, count, true);
}

uintptr_t alloc_frames_dma32(size_t count) {
    if (count == 0)
        return 0;

    size_t required_pages = 0;
    if (!count_to_pages(count, &required_pages))
        return 0;

    uintptr_t addr = 0;

    if (zones[ZONE_DMA32] && zone_has_memory(zones[ZONE_DMA32])) {
        addr = buddy_alloc_zone(zones[ZONE_DMA32], count);
        if (addr != 0)
            goto out;
    }

#if defined(__x86_64__)
    if (zones[ZONE_DMA] && zone_has_memory(zones[ZONE_DMA])) {
        addr = buddy_alloc_zone(zones[ZONE_DMA], count);
        if (addr != 0)
            goto out;
    }
#endif

out:
    if (addr == 0)
        return 0;

    size_t page_index = addr / PAGE_SIZE;
    bitmap_set_range(&using_regions, page_index, page_index + required_pages, true);

    for (size_t offset = 0; offset < required_pages; offset++) {
        page_t *page = get_page_ref(addr + offset * PAGE_SIZE);
        if (page)
            page_ref(page);
    }

    return addr;
}

void free_frames_dma32(uintptr_t addr, size_t count) {
    free_frames(addr, count);
}
