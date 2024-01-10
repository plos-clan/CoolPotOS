#ifndef CPOS_KHEAP_H
#define CPOS_KHEAP_H

#include <stddef.h>
#include <stdint.h>

#define KHEAP_START 0xC0000000
#define KHEAP_INITIAL_SIZE 0xF00000

void *ksbrk(int incr);

typedef char ALIGN[16];

typedef union header {
    struct {
        uint32_t size;
        uint32_t is_free;
        union header *next;
    } s;
    ALIGN stub;
} header_t;

void *alloc(uint32_t size);
void kfree(void *block);

uint32_t kmalloc_a(uint32_t size);
uint32_t kmalloc_p(uint32_t size, uint32_t *phys);
uint32_t kmalloc_ap(uint32_t size, uint32_t *phys);
uint32_t kmalloc(uint32_t size);

#endif
