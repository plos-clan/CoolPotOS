#include "kasan.h"
#include "krlibc.h"
#include "mem/page.h"
#include "mem/frame.h"
#include "term/klog.h"

#if KASAN_CHECK

#    define KASAN_SHADOW_SCALE_SHIFT 3
#    define KASAN_SHADOW_GRANULE (1UL << KASAN_SHADOW_SCALE_SHIFT)

#    define KASAN_MAX_RANGES 8
#    define KASAN_MAX_WHITELIST 8
#    define KASAN_POISON 0xFF

typedef struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t shadow_base;
    uintptr_t shadow_end;
    size_t shadow_size;
    size_t shadow_map_size;
} kasan_range_t;

typedef struct {
    uintptr_t start;
    uintptr_t end;
} kasan_whitelist_t;

static kasan_range_t kasan_ranges[KASAN_MAX_RANGES];
static size_t kasan_range_count = 0;
static kasan_whitelist_t kasan_whitelist[KASAN_MAX_WHITELIST];
static size_t kasan_whitelist_count  = 0;
static uintptr_t kasan_shadow_cursor = KASAN_SHADOW_BASE;
static bool kasan_initialized        = false;
static bool kasan_active             = false;
static int kasan_heap_range          = -1;
static int kasan_disable_depth       = 0;

static inline uintptr_t align_up(uintptr_t v, uintptr_t a) {
    return (v + a - 1) & ~(a - 1);
}

static inline uintptr_t align_down(uintptr_t v, uintptr_t a) {
    return v & ~(a - 1);
}

static void kasan_memset_u8(uint8_t *dst, uint8_t val, size_t size) {
    for (size_t i = 0; i < size; i++) {
        dst[i] = val;
    }
}

static inline uintptr_t kasan_shadow_addr(const kasan_range_t *range, uintptr_t addr) {
    return range->shadow_base + ((addr - range->start) >> KASAN_SHADOW_SCALE_SHIFT);
}

static bool kasan_is_whitelisted(uintptr_t start, uintptr_t end) {
    for (size_t i = 0; i < kasan_whitelist_count; i++) {
        if (start >= kasan_whitelist[i].start && end <= kasan_whitelist[i].end) {
            return true;
        }
    }
    return false;
}

static void kasan_add_whitelist(uintptr_t start, uintptr_t end) {
    if (kasan_whitelist_count >= KASAN_MAX_WHITELIST)
        return;
    kasan_whitelist[kasan_whitelist_count].start = start;
    kasan_whitelist[kasan_whitelist_count].end   = end;
    kasan_whitelist_count++;
}

static int kasan_add_range(uintptr_t start, uintptr_t end, bool poison_initial) {
    if (start >= end || kasan_range_count >= KASAN_MAX_RANGES)
        return -1;
    size_t size           = end - start;
    size_t shadow_size    = (size + KASAN_SHADOW_GRANULE - 1) >> KASAN_SHADOW_SCALE_SHIFT;
    uintptr_t shadow_base = align_up(kasan_shadow_cursor, PAGE_SIZE);
    size_t shadow_map     = align_up(shadow_size, PAGE_SIZE);

    page_map_range_to_random(get_kernel_pagedir(), shadow_base, shadow_map, KERNEL_PTE_FLAGS);
    kasan_memset_u8((uint8_t *)shadow_base, poison_initial ? KASAN_POISON : 0, shadow_size);

    kasan_ranges[kasan_range_count] = (kasan_range_t){
        .start           = start,
        .end             = end,
        .shadow_base     = shadow_base,
        .shadow_end      = shadow_base + shadow_map,
        .shadow_size     = shadow_size,
        .shadow_map_size = shadow_map,
    };
    int idx = (int)kasan_range_count;
    kasan_range_count++;
    kasan_shadow_cursor = shadow_base + shadow_map;
    return idx;
}

static void
kasan_report(const char *reason, uintptr_t bad, size_t size, bool is_write, uint8_t shadow_val) {
    kasan_disable_depth++;
    logkf("\n[KASAN] INVALID %s access\n", is_write ? "WRITE" : "READ");
    logkf(
        "[KASAN] addr=%p size=%llu shadow=0x%02X\n", (void *)bad, (unsigned long long)size,
        shadow_val
    );
    if (reason)
        logkf("[KASAN] from=%s\n", reason);
    arch_close_interrupt();
    while (true) {
        arch_wait_for_interrupt();
    }
}

static void kasan_check_range_in_range(
    const kasan_range_t *range, uintptr_t start, uintptr_t end, bool is_write, const char *reason
) {
    uintptr_t cur = start;
    while (cur < end) {
        uintptr_t block = cur & ~(KASAN_SHADOW_GRANULE - 1);
        uint8_t shadow  = *(uint8_t *)kasan_shadow_addr(range, block);

        if (shadow == 0) {
            cur = block + KASAN_SHADOW_GRANULE;
            continue;
        }
        if (shadow >= 0x80) {
            kasan_report(reason, cur, end - start, is_write, shadow);
        }

        uintptr_t valid_end = block + shadow;
        if (cur < valid_end && end <= valid_end) {
            cur = block + KASAN_SHADOW_GRANULE;
            continue;
        }
        uintptr_t bad = (cur < valid_end) ? valid_end : cur;
        kasan_report(reason, bad, end - start, is_write, shadow);
    }
}

static void kasan_poison_range(const kasan_range_t *range, uintptr_t start, uintptr_t end) {
    if (start >= end)
        return;
    uintptr_t s = align_down(start, KASAN_SHADOW_GRANULE);
    uintptr_t e = align_up(end, KASAN_SHADOW_GRANULE);
    for (uintptr_t addr = s; addr < e; addr += KASAN_SHADOW_GRANULE) {
        uint8_t *shadow = (uint8_t *)kasan_shadow_addr(range, addr);
        *shadow         = KASAN_POISON;
    }
}

static void kasan_unpoison_range(const kasan_range_t *range, uintptr_t start, uintptr_t end) {
    if (start >= end)
        return;

    uintptr_t block_start = align_down(start, KASAN_SHADOW_GRANULE);
    uintptr_t block_end   = align_down(end, KASAN_SHADOW_GRANULE);

    if (block_start == block_end) {
        uint8_t *shadow = (uint8_t *)kasan_shadow_addr(range, block_start);
        if ((start & (KASAN_SHADOW_GRANULE - 1)) == 0) {
            size_t tail = end - block_start;
            *shadow     = (tail >= KASAN_SHADOW_GRANULE) ? 0 : (uint8_t)tail;
        } else {
            *shadow = 0;
        }
        return;
    }

    *(uint8_t *)kasan_shadow_addr(range, block_start) = 0;
    for (uintptr_t addr = block_start + KASAN_SHADOW_GRANULE; addr < block_end;
         addr += KASAN_SHADOW_GRANULE) {
        *(uint8_t *)kasan_shadow_addr(range, addr) = 0;
    }

    if (end != block_end) {
        size_t tail         = end - block_end;
        uint8_t *shadow_end = (uint8_t *)kasan_shadow_addr(range, block_end);
        *shadow_end         = (tail >= KASAN_SHADOW_GRANULE) ? 0 : (uint8_t)tail;
    }
}

void kasan_init(void) {
    if (kasan_initialized)
        return;
    kasan_shadow_cursor = KASAN_SHADOW_BASE;

    kasan_add_whitelist(0, KERNEL_AREA_MEM);
    kasan_add_whitelist(DRIVER_AREA_MEM, KASAN_SHADOW_BASE);
    kasan_add_whitelist(KASAN_SHADOW_BASE, KASAN_SHADOW_BASE + 0x1000000000UL);
    uint64_t hhdm = get_physical_memory_offset();
    if (hhdm != 0 && hhdm < KERNEL_HEAP_START) {
        kasan_add_whitelist(hhdm, KERNEL_HEAP_START);
    }

    int kernel_idx = kasan_add_range(KASAN_MONITOR_START, KASAN_MONITOR_END, false);
    if (kernel_idx < 0) {
        logkf("[KASAN] failed to map kernel shadow\n");
    }

    kasan_initialized = true;
    kasan_active      = true;
    logkf(
        "[KASAN] enabled. shadow_base=%p ranges=%llu whitelist=%llu\n", (void *)KASAN_SHADOW_BASE,
        (unsigned long long)kasan_range_count, (unsigned long long)kasan_whitelist_count
    );
}

bool kasan_is_active(void) {
    return kasan_active && kasan_disable_depth == 0;
}

void kasan_push_disable(void) {
    kasan_disable_depth++;
}

void kasan_pop_disable(void) {
    if (kasan_disable_depth > 0)
        kasan_disable_depth--;
}

void kasan_heap_init(uintptr_t heap_start, size_t heap_size) {
    if (!kasan_initialized)
        kasan_init();
    if (kasan_heap_range >= 0 || heap_size == 0)
        return;

    int idx = kasan_add_range(heap_start, heap_start + heap_size, true);
    if (idx < 0) {
        logkf("[KASAN] failed to map heap shadow\n");
        return;
    }
    kasan_heap_range = idx;
}

void kasan_heap_extend(uintptr_t heap_start, size_t heap_size) {
    if (!kasan_initialized || heap_size == 0)
        return;
    if (kasan_heap_range < 0) {
        kasan_heap_init(heap_start, heap_size);
        return;
    }

    kasan_range_t *range = &kasan_ranges[kasan_heap_range];
    uintptr_t new_end    = heap_start + heap_size;

    if (heap_start != range->end) {
        kasan_add_range(heap_start, new_end, true);
        return;
    }

    size_t new_shadow_size =
        (new_end - range->start + KASAN_SHADOW_GRANULE - 1) >> KASAN_SHADOW_SCALE_SHIFT;
    if (new_shadow_size <= range->shadow_size) {
        range->end = new_end;
        return;
    }

    size_t new_shadow_map = align_up(new_shadow_size, PAGE_SIZE);
    if (new_shadow_map > range->shadow_map_size) {
        uintptr_t map_addr = range->shadow_base + range->shadow_map_size;
        size_t map_len     = new_shadow_map - range->shadow_map_size;
        page_map_range_to_random(get_kernel_pagedir(), map_addr, map_len, KERNEL_PTE_FLAGS);
        range->shadow_map_size = new_shadow_map;
        range->shadow_end      = range->shadow_base + range->shadow_map_size;
        if (kasan_shadow_cursor < range->shadow_end) {
            kasan_shadow_cursor = range->shadow_end;
        }
    }

    kasan_memset_u8(
        (uint8_t *)(range->shadow_base + range->shadow_size), KASAN_POISON,
        new_shadow_size - range->shadow_size
    );
    range->shadow_size = new_shadow_size;
    range->end         = new_end;
}

void kasan_poison(const void *addr, size_t size) {
    if (!kasan_initialized || size == 0 || addr == NULL)
        return;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end   = start + size;
    if (end < start)
        return;

    for (size_t i = 0; i < kasan_range_count; i++) {
        kasan_range_t *range = &kasan_ranges[i];
        if (end <= range->start || start >= range->end)
            continue;
        uintptr_t rs = start < range->start ? range->start : start;
        uintptr_t re = end > range->end ? range->end : end;
        kasan_poison_range(range, rs, re);
    }
}

void kasan_unpoison(const void *addr, size_t size) {
    if (!kasan_initialized || size == 0 || addr == NULL)
        return;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end   = start + size;
    if (end < start)
        return;

    for (size_t i = 0; i < kasan_range_count; i++) {
        kasan_range_t *range = &kasan_ranges[i];
        if (end <= range->start || start >= range->end)
            continue;
        uintptr_t rs = start < range->start ? range->start : start;
        uintptr_t re = end > range->end ? range->end : end;
        kasan_unpoison_range(range, rs, re);
    }
}

void kasan_check_range(const void *addr, size_t size, bool is_write, const char *reason) {
    if (!kasan_is_active() || size == 0 || addr == NULL)
        return;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end   = start + size;
    if (end < start) {
        kasan_report(reason, start, size, is_write, 0xFF);
        return;
    }

    if (kasan_is_whitelisted(start, end))
        return;

    bool fully_covered = false;
    for (size_t i = 0; i < kasan_range_count; i++) {
        if (start >= kasan_ranges[i].start && end <= kasan_ranges[i].end) {
            fully_covered = true;
            break;
        }
    }

    if (!fully_covered) {
        kasan_report(reason, start, size, is_write, 0xFE);
        return;
    }

    for (size_t i = 0; i < kasan_range_count; i++) {
        const kasan_range_t *range = &kasan_ranges[i];
        if (start >= range->start && end <= range->end) {
            kasan_check_range_in_range(range, start, end, is_write, reason);
            return;
        }
    }
}

#endif
