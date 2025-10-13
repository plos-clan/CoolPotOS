#include "buddy.h"
#include "boot.h"
#include "frame.h"
#include "hhdm.h"
#include "klog.h"
#include "krlibc.h"
#include "page.h"

static buddy_frame_t *bframe          = NULL;
static size_t         total_frames    = 0;
static size_t         metadata_frames = 0;
static size_t         free_list_heads[MAX_ORDER];
static size_t         origin_frames = 0;
static size_t         usable_frames = 0;
extern FrameAllocator frame_allocator;

static inline void free_list_init(void) {
    for (int i = 0; i < MAX_ORDER; i++)
        free_list_heads[i] = INVALID_INDEX;
}

static void free_list_push(int order, size_t idx) {
    bframe[idx].order      = order;
    bframe[idx].is_free    = true;
    bframe[idx].next       = free_list_heads[order];
    free_list_heads[order] = idx;
}

static bool free_list_remove(int order, size_t target_idx) {
    size_t *p = &free_list_heads[order];
    while (*p != INVALID_INDEX) {
        if (*p == target_idx) {
            *p                         = bframe[*p].next;
            bframe[target_idx].next    = INVALID_INDEX;
            bframe[target_idx].is_free = false;
            return true;
        }
        p = &bframe[*p].next;
    }
    return false;
}

static inline size_t buddy_of(size_t idx, int order) {
    return idx ^ ((size_t)1 << order);
}

static size_t buddy_alloc_order(int order) {
    if (order < 0 || order >= MAX_ORDER) return INVALID_INDEX;

    int cur = order;
    while (cur < MAX_ORDER && free_list_heads[cur] == INVALID_INDEX)
        cur++;

    if (cur >= MAX_ORDER) return INVALID_INDEX;

    size_t idx           = free_list_heads[cur];
    free_list_heads[cur] = bframe[idx].next;
    bframe[idx].next     = INVALID_INDEX;
    bframe[idx].is_free  = false;

    while (cur > order) {
        cur--;
        size_t buddy_idx = idx + ((size_t)1 << cur);
        free_list_push(cur, buddy_idx);
        bframe[idx].order = cur;
    }
    bframe[idx].is_free = false;
    bframe[idx].order   = order;
    return idx;
}

static void buddy_free_order(size_t idx, int order) {
    if (idx >= total_frames || order < 0 || order >= MAX_ORDER) return;

    size_t cur_idx   = idx;
    int    cur_order = order;

    while (cur_order < MAX_ORDER - 1) {
        size_t b = buddy_of(cur_idx, cur_order);

        if (b >= total_frames) break;
        if (!bframe[b].is_free) break;
        if (bframe[b].order != (uint32_t)cur_order) break;
        if (bframe[b].region_id != bframe[cur_idx].region_id) break;

        bool removed = free_list_remove(cur_order, b);
        if (!removed) break;

        if (b < cur_idx) cur_idx = b;
        cur_order++;
    }

    free_list_push(cur_order, cur_idx);
}

static int order_for_pages(size_t n_pages) {
    //int    ord  = 0;
    int    ord  = 1;
    size_t size = 1;
    while (size < n_pages && ord < MAX_ORDER) {
        size <<= 1;
        ord++;
    }
    return ord;
}

size_t buddy_alloc_pages(size_t n_pages) {
    if (n_pages == 0) return (size_t)-1;
    int    ord = order_for_pages(n_pages);
    size_t idx = buddy_alloc_order(ord);
    if (idx == INVALID_INDEX) return (size_t)-1;
    return idx * PAGE_SIZE;
}

void buddy_free_pages(size_t phys_addr, size_t n_pages) {
    if (phys_addr % PAGE_SIZE) return;
    if (phys_addr == 0) {
        if (n_pages < 2) return;
        phys_addr += PAGE_SIZE;
    }
    size_t idx = phys_addr / PAGE_SIZE;
    int    ord = order_for_pages(n_pages);
    buddy_free_order(idx, ord);
}
static inline size_t highest_pow2_le(size_t n) {
    if (n == 0) return 0;
    return (size_t)1 << (63 - __builtin_clzll(n));
}
static inline int ilog2(size_t x) {
    return (63 - __builtin_clzll(x));
}

static void add_free_range(size_t start_frame, size_t count, int region_id) {
    size_t cur    = start_frame;
    size_t remain = count;

    while (remain > 0) {
        // 跳过 frame0
        if (cur == 0) {
            cur++;
            remain--;
            continue;
        }

        size_t p2 = highest_pow2_le(remain);
        while (p2 > 0 && ((cur & (p2 - 1)) != 0))
            p2 >>= 1;

        if (p2 == 0) {
            free_list_push(0, cur);
            cur    += 1;
            remain -= 1;
        } else {
            int ord = ilog2(p2);
            free_list_push(ord, cur);
            cur    += p2;
            remain -= p2;
        }
    }
}

static bool try_alloc_specific(int order, size_t idx) {
    if (order < 0 || order >= MAX_ORDER) return false;
    if ((idx & (((size_t)1 << order) - 1)) != 0) return false;

    for (int cur = order; cur < MAX_ORDER; cur++) {
        size_t anc = idx & ~(((size_t)1 << cur) - 1);
        if (!free_list_remove(cur, anc)) continue;
        size_t cur_idx = anc;
        for (int c = cur; c > order; c--) {
            int    child_order = c - 1;
            size_t half        = ((size_t)1 << child_order);
            size_t left        = cur_idx;
            size_t right       = cur_idx + half;
            if (idx < right) {
                free_list_push(child_order, right);
            } else {
                free_list_push(child_order, left);
                cur_idx = right;
            }
        }
        bframe[idx].is_free = false;
        bframe[idx].order   = order;
        bframe[idx].next    = INVALID_INDEX;
        return true;
    }
    return false;
}

static bool alloc_contiguous_blocks_order(size_t start_block_index, size_t block_count, int order_k,
                                          uint64_t *out_first_phys) {
    size_t block_frames = ((size_t)1 << order_k);
    size_t start_frame  = start_block_index * block_frames;
    size_t remaining    = block_count;
    size_t cur          = start_frame;

    size_t saved_idx[128];
    int    saved_ord[128];
    size_t saved_cnt = 0;

    while (remaining > 0) {
        size_t p2 = highest_pow2_le(remaining);
        while (p2 > 0 && ((cur & (p2 * block_frames - 1)) != 0))
            p2 >>= 1;
        if (p2 == 0) p2 = 1;

        bool   allocated = false;
        size_t trial     = p2;
        while (trial > 0) {
            int ord = order_k + ilog2(trial);
            if (ord >= MAX_ORDER) {
                trial >>= 1;
                continue;
            }

            if (try_alloc_specific(ord, cur)) {
                if (saved_cnt >= sizeof(saved_idx) / sizeof(saved_idx[0])) {
                    for (size_t j = 0; j < saved_cnt; j++)
                        buddy_free_order(saved_idx[j], saved_ord[j]);
                    return false;
                }
                saved_idx[saved_cnt] = cur;
                saved_ord[saved_cnt] = ord;
                saved_cnt++;
                cur       += trial * block_frames;
                remaining -= trial;
                allocated  = true;
                break;
            }
            trial >>= 1;
        }

        if (!allocated) {
            for (size_t j = 0; j < saved_cnt; j++)
                buddy_free_order(saved_idx[j], saved_ord[j]);
            return false;
        }
    }

    if (out_first_phys != NULL) *out_first_phys = (uint64_t)start_frame * PAGE_SIZE;
    return true;
}

static uint64_t alloc_frames_blocksize(int order_k, size_t count) {
    if (count == 0) return 0;
    size_t block_frames = ((size_t)1 << order_k);
    size_t total_blocks = total_frames / block_frames;
    if (total_blocks == 0) return 0;
    if (count > total_blocks) return 0;

    for (size_t start = 0; start + count <= total_blocks; start++) {
        uint64_t phys        = 0;
        if (alloc_contiguous_blocks_order(start, count, order_k, &phys)) { return phys; }
    }
    return 0;
}

static void free_frames_blocksize(int order_k, uint64_t addr) {
    if (addr % PAGE_SIZE) return;
    size_t start_frame = addr / PAGE_SIZE;
    buddy_free_order(start_frame, order_k);
}

uint64_t buddy_alloc_frames_2M(size_t count) {
    return alloc_frames_blocksize(ORDER_2M, count);
}

uint64_t buddy_alloc_frames_1G(size_t count) {
    return alloc_frames_blocksize(ORDER_1G, count);
}

void buddy_free_frames_2M(uint64_t addr) {
    free_frames_blocksize(ORDER_2M, addr);
}

void buddy_free_frames_1G(uint64_t addr) {
    free_frames_blocksize(ORDER_1G, addr);
}

void init_frame_buddy(uint64_t memory_size) {
    struct limine_memmap_response *memory_map = get_memory_map();

    if (!memory_map) return;

    total_frames = memory_size / PAGE_SIZE;
    if (total_frames == 0) return;

    size_t pages_array_size = total_frames * sizeof(buddy_frame_t);
    metadata_frames         = (pages_array_size + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t meta_phys = 0;
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        if (region->type == LIMINE_MEMMAP_USABLE &&
            region->length >= (uint64_t)metadata_frames * PAGE_SIZE) {
            meta_phys = region->base;
            break;
        }
    }
    if (!meta_phys) {
        logkf("buddy: no region to place metadata\n");
        return;
    }

    bframe = (buddy_frame_t *)phys_to_virt(meta_phys);
    memset(bframe, 0, metadata_frames * PAGE_SIZE);

    free_list_init();

    origin_frames = 0;
    usable_frames = 0;
    extern uint64_t all_memory;
    extern uint64_t reserved_memory;
    extern uint64_t bad_memory;

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        uint64_t                    base   = region->base;
        uint64_t                    len    = region->length;

        switch (region->type) {
        case LIMINE_MEMMAP_USABLE: {
            size_t start_frame  = base / PAGE_SIZE;
            size_t frame_count  = len / PAGE_SIZE;
            origin_frames      += frame_count;
            all_memory         += len;
            break;
        }
        case LIMINE_MEMMAP_RESERVED:
        case LIMINE_MEMMAP_ACPI_NVS:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            reserved_memory += region->length;
            all_memory      += region->length;
            break;
        case LIMINE_MEMMAP_BAD_MEMORY:
            bad_memory += region->length;
            all_memory += region->length;
            break;
        default: all_memory += region->length; break;
        }
    }

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        if (region->type != LIMINE_MEMMAP_USABLE) continue;

        size_t start_frame = region->base / PAGE_SIZE;
        size_t frame_count = region->length / PAGE_SIZE;

        size_t meta_start = meta_phys / PAGE_SIZE;
        size_t meta_count = metadata_frames;

        size_t seg_start = start_frame;
        size_t seg_end   = start_frame + frame_count;

        if (meta_start >= seg_end || (meta_start + meta_count) <= seg_start) {
            add_free_range(seg_start, frame_count, (int)i);
            usable_frames += frame_count;
        } else {
            if (meta_start > seg_start) {
                size_t left_count = meta_start - seg_start;
                add_free_range(seg_start, left_count, (int)i);
                usable_frames += left_count;
            }
            size_t after_start = meta_start + meta_count;
            if (after_start < seg_end) {
                size_t right_count = seg_end - after_start;
                add_free_range(after_start, right_count, (int)i);
                usable_frames += right_count;
            }
        }
    }

    reserved_memory               += metadata_frames * PAGE_SIZE;
    frame_allocator.origin_frames  = origin_frames;
    frame_allocator.usable_frames  = usable_frames;
    logkf("buddy: total frames = %zu, metadata_frames = %zu, usable_frames = %zu\n", total_frames,
          metadata_frames, usable_frames);
}
