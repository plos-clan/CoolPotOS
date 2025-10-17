#include "memstat.h"
#include "mem/frame.h"
#include "mem/page.h"

uint64_t reserved_memory = 0;
uint64_t bad_memory      = 0;
uint64_t all_memory      = 0;

uint64_t get_reserved_memory() {
    return reserved_memory;
}

uint64_t get_all_memory() {
    return all_memory;
}

uint64_t get_available_memory() {
    return frame_allocator.usable_frames * PAGE_SIZE;
}

uint64_t get_used_memory() {
    return (frame_allocator.origin_frames - frame_allocator.usable_frames) * PAGE_SIZE;
}

uint64_t get_bad_memory() {
    return bad_memory;
}
