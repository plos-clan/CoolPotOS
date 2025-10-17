#pragma once

#define KILO_SHIFT 10
#define MEGA_SHIFT 20
#define GIGA_SHIFT 30

#define KILO_FACTOR (1UL << KILO_SHIFT) // 1024
#define MEGA_FACTOR (1UL << MEGA_SHIFT) // 1048576
#define GIGA_FACTOR (1UL << GIGA_SHIFT) // 1073741824

#include "limine.h"
#include "types.h"

typedef struct {
    void  *allocator;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

struct limine_memmap_response *get_memory_map();
uint64_t                       get_memory_size();

void     init_frame();
uint64_t get_physical_memory_offset();
void    *phys_to_virt(uint64_t phys_addr);
uint64_t virt_to_phys(void *virt_addr);
