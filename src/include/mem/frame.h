#pragma once

#define KILO_SHIFT 10
#define MEGA_SHIFT 20
#define GIGA_SHIFT 30

#define KILO_FACTOR (1UL << KILO_SHIFT) // 1024
#define MEGA_FACTOR (1UL << MEGA_SHIFT) // 1048576
#define GIGA_FACTOR (1UL << GIGA_SHIFT) // 1073741824

#include "metadata.h"
#include "types.h"

typedef struct {
    void *allocator;
    size_t total_frames;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

uint64_t get_memory_size();

void init_frame();
uint64_t alloc_frames(size_t count);
uint64_t alloc_frames_2M(size_t count);
uint64_t alloc_frames_1G(size_t count);
void free_frames(uint64_t addr, size_t count);
void free_frame(uint64_t addr);
void free_frames_2M(uint64_t addr);
void free_frames_1G(uint64_t addr);

uint64_t get_physical_memory_offset();
void *phys_to_virt(uint64_t phys_addr);
uint64_t virt_to_phys(void *virt_addr);
void *driver_phys_to_virt(uint64_t phys_addr);
uint64_t driver_virt_to_phys(void *virt_addr);

static inline bool check_user_overflow(uint64_t addr, uint64_t size) {
    return (addr + size) > KERNEL_AREA_MEM;
}
