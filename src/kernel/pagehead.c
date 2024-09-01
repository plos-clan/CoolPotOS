#include "../include/pagehead.h"
#include "../include/memory.h"

void *page_break, *page_end;
header_t *page_head;
header_t *page_tail;
extern void *program_break;
extern page_directory_t *kernel_directory;

static header_t *page_get_free_block(size_t size) {
    header_t *curr = page_head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void *page_sbrk(int incr) {
    if (page_break + incr >= (page_end)) return (void *) -1;
    void *prev_break = page_break;
    page_break += incr;
    return prev_break;
}

void *page_malloc(size_t size,uint32_t *phys){
    void *addr = page_alloc(size);
    *phys = addr;
    if (program_break) {
        if (phys) {
            page_t *page = get_page((uint32_t) addr, 0, kernel_directory,false);
            *phys = page->frame * 0x1000 + ((uint32_t) addr & 0x00000FFF);
        }
    }
    return (uint32_t) addr;
}

void *page_alloc(size_t size) {
    uint32_t total_size;
    void *block;
    header_t *header;
    if (!size) return NULL;
    header = page_get_free_block(size);
    if (header) {
        header->s.is_free = 0;
        return (void *) (header + 1);
    }
    total_size = sizeof(header_t) + size;
    block = page_sbrk(total_size);
    if (block == (void *) -1) return NULL;
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if (!page_head) page_head = header;
    if (page_tail) page_tail->s.next = header;
    page_tail = header;
    return (void *) (header + 1);
}

void page_free(void *block) {
    header_t *header, *tmp;
    if (!block) return;
    header = (header_t *) block - 1;
    if ((char *) block + header->s.size == page_break) {
        if (page_head == page_tail)page_head = page_tail = NULL;
        else {
            tmp = page_head;
            while (tmp) {
                if (tmp->s.next == page_tail) {
                    tmp->s.next = NULL;
                    page_tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        page_sbrk(0 - sizeof(header_t) - header->s.size);
        return;
    }
    header->s.is_free = 1;
}
