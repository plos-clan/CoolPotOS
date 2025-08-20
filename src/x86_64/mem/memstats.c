#include "memstats.h"
#include "frame.h"
#include "lazyalloc.h"
#include "lock_queue.h"
#include "page.h"
#include "pcb.h"

extern FrameAllocator frame_allocator;
extern lock_queue    *pgb_queue;

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
    spin_lock(pgb_queue->lock);
    queue_foreach(pgb_queue, node) {
        pcb_t process = (pcb_t)node->data;
        if (process->status == DEATH || process->status == OUT) continue;
        spin_lock(process->virt_queue->lock);
        queue_foreach(process->virt_queue, node0) {
            mm_virtual_page_t *vpage  = (mm_virtual_page_t *)node0->data;
            commit_memory            += vpage->count * PAGE_SIZE;
        }
        spin_unlock(process->virt_queue->lock);
    }
    spin_unlock(pgb_queue->lock);
    return commit_memory;
}
