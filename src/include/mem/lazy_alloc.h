#pragma once

#include "llist_queue.h"
#include "task/task.h"

typedef struct mm_virtual_page {
    uint64_t     start;
    uint64_t     count;
    uint64_t     flags;
    uint64_t     pte_flags;
    list_node_t *index;
} mm_virtual_page_t;

errno_t lazy_tryalloc(pcb_t pcb, uint64_t address);
void lazy_infoalloc(pcb_t process, uint64_t vaddr, size_t length, uint64_t page_flags,
                       uint64_t flags);
void lazy_free(pcb_t process);
void unmap_virtual_page(pcb_t process, uint64_t vaddr, size_t length);
