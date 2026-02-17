#include "mem/lazy_alloc.h"
#include "errno.h"
#include "mem/frame.h"

void *virt_copy(void *ptr) {
    if (ptr == NULL)
        return NULL;
    mm_virtual_page_t *src_page = (mm_virtual_page_t *)ptr;
    mm_virtual_page_t *new_page = (mm_virtual_page_t *)malloc(sizeof(mm_virtual_page_t));
    new_page->start             = src_page->start;
    new_page->flags             = src_page->flags;
    new_page->count             = src_page->count;
    new_page->pte_flags         = src_page->pte_flags;
    // new_page->index             = src_page->index;
    return new_page;
}

void virt_copy_index(void *ptr, list_node_t *node) {
    mm_virtual_page_t *src_page = (mm_virtual_page_t *)ptr;
    src_page->index             = node;
}

#include "term/klog.h"
USED void debug_print_virt_queue(pcb_t pcb) {
    qlist_foreach(pcb->virt_queue, node) {
        mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
        logkf("%p", vpage);
    }
}

errno_t lazy_tryalloc(pcb_t pcb, uint64_t address) {
    if (unlikely(address == 0))
        return -1;
    mm_virtual_page_t *virt_page = NULL;
    qlist_foreach(pcb->virt_queue, node) {
        mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
        if (address >= vpage->start && address < vpage->start + vpage->count * PAGE_SIZE) {
            virt_page = vpage;
            break;
        }
    }

    if (virt_page == NULL) {
        return -1;
    } else {
        size_t   fault_index = (address - virt_page->start) / PAGE_SIZE;
        uint64_t page_addr   = virt_page->start + fault_index * PAGE_SIZE;

        uint64_t phys = alloc_frames(1);
        page_map_to(get_current_directory(), page_addr, phys, virt_page->pte_flags);
        memset((void *)page_addr, 0, PAGE_SIZE);

        uint64_t range_start = virt_page->start;
        // uint64_t range_end = range_start + virt_page->count * PAGE_SIZE;

        mm_virtual_page_t *left  = NULL;
        mm_virtual_page_t *right = NULL;

        // 左侧未分配部分
        if (fault_index > 0) {
            left            = malloc(sizeof(mm_virtual_page_t));
            left->start     = range_start;
            left->count     = fault_index;
            left->pte_flags = virt_page->pte_flags;
            left->index     = list_enqueue(pcb->virt_queue, left);
        }

        // 右侧未分配部分
        if (fault_index + 1 < virt_page->count) {
            right            = malloc(sizeof(mm_virtual_page_t));
            right->start     = range_start + (fault_index + 1) * PAGE_SIZE;
            right->count     = virt_page->count - fault_index - 1;
            right->pte_flags = virt_page->pte_flags;
            right->index     = list_enqueue(pcb->virt_queue, right);
        }

        // 移除原始 mm_virtual_page
        list_remove_node(pcb->virt_queue, virt_page->index);
        free(virt_page);
        return EOK;
    }
}

void unmap_virtual_page(pcb_t process, uint64_t vaddr, size_t length) {
    uint64_t vaddr_end = vaddr + length;
    do {
        mm_virtual_page_t *vpage = NULL;
        spin_lock(process->virt_queue->lock);
        qlist_foreach(process->virt_queue, node) {
            mm_virtual_page_t *virtual_page = (mm_virtual_page_t *)node->data;
            uint64_t           start        = virtual_page->start;
            uint64_t           end          = virtual_page->start + virtual_page->count * PAGE_SIZE;

            if (end > vaddr && start < vaddr_end) {
                vpage = virtual_page;
                break;
            }
        }
        spin_unlock(process->virt_queue->lock);
        if (vpage == NULL)
            break;
        uint64_t start = vpage->start;
        uint64_t end   = vpage->start + vpage->count * PAGE_SIZE;

        if (vaddr <= start && vaddr_end >= end) {
            // 完全覆盖
            goto free_end;
        }
        if (vaddr <= start && vaddr_end < end) {
            // 覆盖左边，保留右段
            mm_virtual_page_t *right = malloc(sizeof(mm_virtual_page_t));
            right->start             = vaddr_end;
            right->count             = (end - vaddr_end) / PAGE_SIZE;
            right->flags             = vpage->flags;
            right->pte_flags         = vpage->pte_flags;
            right->index             = list_enqueue(process->virt_queue, right);
            goto free_end;
        }

        if (vaddr > start && vaddr_end >= end) {
            // 覆盖右边，保留左段
            mm_virtual_page_t *left = malloc(sizeof(mm_virtual_page_t));
            left->start             = start;
            left->count             = (vaddr - start) / PAGE_SIZE;
            left->flags             = vpage->flags;
            left->pte_flags         = vpage->pte_flags;
            left->index             = list_enqueue(process->virt_queue, left);
            goto free_end;
        }

        if (vaddr > start && vaddr_end < end) {
            // 中间部分覆盖，要拆成两段
            mm_virtual_page_t *left = malloc(sizeof(mm_virtual_page_t));
            left->start             = start;
            left->count             = (vaddr - start) / PAGE_SIZE;
            left->flags             = vpage->flags;
            left->pte_flags         = vpage->pte_flags;
            left->index             = list_enqueue(process->virt_queue, left);

            mm_virtual_page_t *right = malloc(sizeof(mm_virtual_page_t));
            right->start             = vaddr_end;
            right->count             = (end - vaddr_end) / PAGE_SIZE;
            right->flags             = vpage->flags;
            right->pte_flags         = vpage->pte_flags;
            right->index             = list_enqueue(process->virt_queue, right);
            goto free_end;
        }
    free_end:
        list_remove_node(process->virt_queue, vpage->index); // 删掉信息
        free(vpage);                                         // 释放信息
        vpage = NULL;
    } while (true);
}

void lazy_infoalloc(
    pcb_t process, uint64_t vaddr, size_t length, uint64_t page_flags, uint64_t flags
) {
    uint64_t count = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (flags & MAP_FIXED) {
        unmap_virtual_page(process, vaddr, length);
        unmap_page_range(get_current_directory(), vaddr, length);
    }

    mm_virtual_page_t *virt_page = malloc(sizeof(mm_virtual_page_t));
    not_null_assert(virt_page, "Out of memory for virtual page allocation");
    virt_page->start     = vaddr;
    virt_page->count     = count;
    virt_page->flags     = flags;
    virt_page->pte_flags = page_flags;
    virt_page->index     = list_enqueue(process->virt_queue, virt_page);
}

void lazy_free(pcb_t process) {
    if (process->virt_queue->size > 0) {
    refree_virt:;
        mm_virtual_page_t *virt_page = NULL;

        qlist_foreach(process->virt_queue, node) {
            mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
            virt_page                = vpage;
            break;
        }
        if (virt_page != NULL) {
            list_remove_node(process->virt_queue, virt_page->index);
            free(virt_page);
            goto refree_virt;
        }
    }
}
