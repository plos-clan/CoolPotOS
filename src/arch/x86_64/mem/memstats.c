#include "memstats.h"
#include "frame.h"
#include "lazyalloc.h"
#include "cow_arraylist.h"
#include "page.h"
#include "pcb.h"

extern FrameAllocator frame_allocator;
extern cow_arraylist   *pgb_queue;

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

uint64_t get_commit_memory() {
    uint64_t commit_memory = 0;
    pcb_t process = NULL;
    cow_foreach(pgb_queue, process) {
        if (process->status == DEATH || process->status == OUT) continue;
        spin_lock(process->virt_queue->lock);
        queue_foreach(process->virt_queue, node0) {
            mm_virtual_page_t *vpage  = (mm_virtual_page_t *)node0->data;
            commit_memory            += vpage->count * PAGE_SIZE;
        }
        spin_unlock(process->virt_queue->lock);
    }
    return commit_memory;
}

uint64_t mem_parse_size(uint8_t *s) {
    if (s == NULL) { return UINT64_MAX; }

    uint8_t *p     = s;
    uint64_t value = 0;

    while (isdigit((uint8_t)*p)) {
        int digit = *p - '0';
        value     = value * 10 + digit;
        p++;
    }
    if (*p == '\0') { return value; }
    char suffix = *p;

    if (suffix >= 'a' && suffix <= 'z') { suffix -= ('a' - 'A'); }

    switch (suffix) {
    case 'K':
        value *= KILO_FACTOR; // 乘以 1024 (2^10)
        break;
    case 'M':
        value *= MEGA_FACTOR; // 乘以 1048576 (2^20)
        break;
    case 'G':
        value *= GIGA_FACTOR; // 乘以 1073741824 (2^30)
        break;
    default: return value;
    }
    if (*(p + 1) != '\0') { return UINT64_MAX; }

    return value;
}
