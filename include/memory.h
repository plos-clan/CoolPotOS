#ifndef CPOS_MEMORY_H
#define CPOS_MEMORY_H

#define PAGE_BASE_ADDRESS 0x400000
#define PAGE_SIZE 4096
#define MAX_DTB_SIZE 0x2000

#include <stddef.h>
#include <stdint.h>
#include "isr.h"

#define IDX(addr) ((unsigned)addr >> 12)            // 获取 addr 的页索引
#define DIDX(addr) (((unsigned)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引
#define TIDX(addr) (((unsigned)addr >> 12) & 0x3ff) // 获取 addr 的页表索引
#define PAGE(idx) ((unsigned)idx << 12)             // 获取页索引 idx 对应的页开始的位置

#define INDEX_FROM_BIT(a) (a / (8 * 4)) // 一个索引对应的位
#define OFFSET_FROM_BIT(a) (a % (8 * 4)) // 一个索引对应的位偏移

typedef struct page {
    uint32_t present: 1; // 是否存在
    uint32_t rw: 1; // 可否读写
    uint32_t user: 1; // 是否可被r3读写
    uint32_t accessed: 1; // 是否被读取过
    uint32_t dirty: 1; // 是否被写入过
    uint32_t unused: 7; // 保留用
    uint32_t frame: 20; // frame地址，但右移12位
} page_t;

typedef struct page_table {
    page_t pages[1024];
} page_table_t; // 页表，PTE，共1024个页

typedef struct page_directory {
    page_table_t *tables[1024]; // 1024个页表
    uint32_t tablesPhysical[1024]; // 1024个页表，但是物理地址
    uint32_t physicalAddr; // tablesPhysical的物理地址
} page_directory_t; // 页目录表，PDE

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst_, const void *src_, uint32_t size);
int memcmp(const void *a_, const void *b_, uint32_t size);

void switch_page_directory(page_directory_t *new); // 切换PDE
page_t *get_page(uint32_t address, int make, page_directory_t *dir); // 获取一个页，若make == 1，则创造一个页，该页指向address
void alloc_frame(page_t *page, int is_kernel, int is_writable);
void page_fault(registers_t *regs);

void flush_tlb();

void init_page();

#endif