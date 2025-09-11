#pragma once

#define PTE_PRESENT    (0x1UL << 0)  // 页面是否存在
#define PTE_WRITEABLE  (0x1UL << 1)  // 页面可写
#define PTE_USER       (0x1UL << 2)  // 页面是否可被用户访问
#define PTE_HUGE       (0x1UL << 7)  // 大页标志 (页表项为 PAT位)
#define PTE_NO_EXECUTE (0x1UL << 63) // 不可执行
#define PTE_DIS_CACHE  (1ULL << 4)   // 禁用缓存
#define PTE_PWT        (1ULL << 3)   // CPU缓存写通策略
#define PTE_U_ACCESSED (1ULL << 5)   // 已访问 (CPU主动标记)
#define PTE_U_DIRTY    (1ULL << 6)   // 已写入 (CPU主动标记)

#define KERNEL_PTE_FLAGS (PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE)

#ifndef PAGE_SIZE
#    define PAGE_SIZE ((size_t)4096)
#endif

#define PAGE_MASK  (~(PAGE_SIZE - 1))
#define ENTRY_MASK 0x1FF

#include "cp_kernel.h"

typedef struct page_table_entry {
    uint64_t value;
} __attribute__((packed)) page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} __attribute__((packed)) page_table_t;

typedef struct page_directory {
    page_table_t *table;
} page_directory_t;

uint64_t          get_physical_memory_offset();
page_directory_t *get_current_directory();

uint64_t alloc_frames(size_t count);
void     free_frames(uint64_t addr, size_t count);

void page_map_range(page_directory_t *dir, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags);
void page_map_range_to(page_directory_t *dir, uint64_t frame, uint64_t length, uint64_t flags);
void switch_process_page_directory(page_directory_t *dir);
page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy);
void              free_page_directory(page_directory_t *dir);

void    *phys_to_virt(uint64_t phys_addr);
void    *virt_to_phys(uint64_t virt_addr);
uint64_t page_virt_to_phys(uint64_t va);
void    *driver_phys_to_virt(uint64_t phys_addr);
void    *driver_virt_to_phys(uint64_t virt_addr);
void     unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);

uint64_t get_reserved_memory();
uint64_t get_all_memory();
uint64_t get_used_memory();
uint64_t get_commit_memory();
