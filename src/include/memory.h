#ifndef CRASHPOWEROS_MEMORY_H
#define CRASHPOWEROS_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "isr.h"

#define KHEAP_INITIAL_SIZE 0xf00000
#define KHEAP_START      0xa0000000
#define STACK_SIZE 32768

#define USER_START 0xb0000000
#define USER_END (USER_START + 0xf00000)
#define USER_HEAP_END (USER_END - STACK_SIZE)

#define INDEX_FROM_BIT(a) (a / (8*4))
#define OFFSET_FROM_BIT(a) (a % (8*4))

typedef char ALIGN[16];

#include "multiboot.h"
#include "common.h"

typedef struct page {
    uint32_t present: 1;
    uint32_t rw: 1;
    uint32_t user: 1;
    uint32_t accessed: 1;
    uint32_t dirty: 1;
    uint32_t unused: 7;
    uint32_t frame: 20;
}__attribute__((packaged)) page_t;

typedef struct page_table {
    page_t pages[1024];
}__attribute__((packaged)) page_table_t;

typedef struct page_directory {
    page_table_t *tables[1024];
    uint32_t tablesPhysical[1024];
    uint32_t physicalAddr;
}__attribute__((packaged)) page_directory_t;

typedef union header {
    struct {
        uint32_t size;
        uint32_t is_free;
        union header *next;
    } s;
    ALIGN stub;
} header_t;

uint32_t memory_usage();

void* memcpy(void* s, const void* ct, size_t n);

int memcmp(const void *a_, const void *b_, uint32_t size);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t num);

void switch_page_directory(page_directory_t *dir);

page_t *get_page(uint32_t address, int make, page_directory_t *dir,bool ist);

void alloc_frame(page_t *page, int is_kernel, int is_writable);

uint32_t first_frame();

void page_fault(registers_t *regs);

page_directory_t *clone_directory(page_directory_t *src);

void flush_tlb();

void *ksbrk(int incr);

void *alloc(uint32_t size);

void kfree(void *ptr);

uint32_t kmalloc_a(uint32_t size);

uint32_t kmalloc_p(uint32_t size, uint32_t *phy);

uint32_t kmalloc(size_t size);

uint32_t kmalloc_ap(uint32_t size, uint32_t *phys);

void init_page(multiboot_t *mboot);

void memclean(char *s, int len);

void *realloc(void *ptr, uint32_t size);

void alloc_frame_line(page_t *page, uint32_t line,int is_kernel, int is_writable);

void free_frame(page_t *page);

void page_flush(page_directory_t *dir);

void page_switch(page_directory_t *dir);

uint32_t kmalloc_i_ap(uint32_t size, uint32_t *phys);

#endif //CRASHPOWEROS_MEMORY_H
