#pragma once

#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif

#define PAGE_SIZE     4096
#define ARCH_PT_LEVEL 4

#define PTE_PRESENT      (0x1UL << 0)  // 页面是否存在
#define PTE_WRITEABLE    (0x1UL << 1)  // 页面可写
#define PTE_USER         (0x1UL << 2)  // 页面是否可被用户访问
#define PTE_HUGE         (0x1UL << 7)  // 大页标志 (页表项为 PAT位)
#define PTE_NO_EXECUTE   (0x1UL << 63) // 不可执行
#define PTE_DIS_CACHE    (1ULL << 4)   // 禁用缓存
#define PTE_PWT          (1ULL << 3)   // CPU缓存写通策略
#define PTE_U_ACCESSED   (1ULL << 5)   // 已访问 (CPU主动标记)
#define PTE_U_DIRTY      (1ULL << 6)   // 已写入 (CPU主动标记)
#define KERNEL_PTE_FLAGS (PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE)

#define PTE_FRAME_MASK 0x000ffffffffff000

#include "types.h"

typedef struct page_table_entry {
    uint64_t value;
} __attribute__((packed)) page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} __attribute__((packed)) page_table_t;

typedef struct page_directory {
    page_table_t *table;
} page_directory_t;

void switch_page_directory(page_directory_t *dir);
page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy);
void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);
void unmap_page(page_directory_t *directory, uint64_t vaddr);
void arch_page_setup();

// 用于构建内核自己的页表, 不再复用引导器提供的页表
void arch_page_setup_l2();
