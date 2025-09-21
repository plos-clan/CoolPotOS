#include "frame.h"
#include "boot.h"
#include "buddy.h"

#define ENABLE_BUDDY 1

FrameAllocator frame_allocator;
uint64_t       memory_size = 0;

void init_frame() {
#ifdef ENABLE_BUDDY
    init_frame_buddy();
#else
    init_frame_bitmap();
#endif
}

void free_frames(uint64_t addr, size_t count) {
#ifdef ENABLE_BUDDY
    buddy_free_pages(addr, count);
#else
    bitmap_free_frames(addr, count);
#endif
    frame_allocator.usable_frames += count;
}

void free_frame(uint64_t addr) {
#ifdef ENABLE_BUDDY
    buddy_free_pages(addr, 1);
#else
    bitmap_free_frame(addr);
#endif
    frame_allocator.usable_frames++;
}

void free_frames_2M(uint64_t addr) {
#ifdef ENABLE_BUDDY
    buddy_free_frames_2M(addr);
#else
    bitmap_free_frames_2M(addr);
#endif
    frame_allocator.usable_frames += 512;
}

void free_frames_1G(uint64_t addr) {
#ifdef ENABLE_BUDDY
    buddy_free_frames_1G(addr);
#else
    bitmap_free_frames_1G(addr);
#endif
    frame_allocator.usable_frames += 262144;
}

uint64_t alloc_frames(size_t count) {
#ifdef ENABLE_BUDDY
    frame_allocator.usable_frames -= count;
    return buddy_alloc_pages(count);
#else
    return bitmap_alloc_frames(count);
#endif
}

uint64_t alloc_frames_2M(size_t count) {
#ifdef ENABLE_BUDDY
    return buddy_alloc_frames_2M(count);
#else
    return bitmap_alloc_frames_2M(count);
#endif
}

uint64_t alloc_frames_1G(size_t count) {
#ifdef ENABLE_BUDDY
    return buddy_alloc_frames_1G(count);
#else
    return bitmap_alloc_frames_1G(count);
#endif
}
