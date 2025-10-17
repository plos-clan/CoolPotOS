#pragma once

#define PAGE_SHIFT 12ULL
#define MAX_ORDER  19 // supports order 0..18 (order 18 => 1 GiB)
#define ORDER_2M   9  // 2 MiB = 2^(12+9)
#define ORDER_1G   18 // 1 GiB = 2^(12+18)

#define INVALID_INDEX ((size_t)-1)

#include "types.h"

typedef struct buddy_frame {
    uint32_t order;
    bool     is_free;
    size_t   next;
    size_t   region_id;
} buddy_frame_t;

void     init_frame_buddy(uint64_t memory_size);
size_t   buddy_alloc_pages(size_t n_pages);
void     buddy_free_pages(size_t phys_addr, size_t n_pages);
uint64_t buddy_alloc_frames_2M(size_t count);
uint64_t buddy_alloc_frames_1G(size_t count);
void     buddy_free_frames_2M(uint64_t addr);
void     buddy_free_frames_1G(uint64_t addr);
