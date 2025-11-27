#pragma once

#ifdef PAGE_SIZE
#    undef PAGE_SIZE
#endif

#define PAGE_SIZE                4096
#define ARCH_PT_LEVEL            4
#define ARCH_PT_OFFSET_BASE      12
#define ARCH_PT_OFFSET_PER_LEVEL 9

#define ARCH_PT_FLAG_VALID     (0x1UL << 0)
#define ARCH_PT_FLAG_DIRTY     (0x1UL << 1)
#define ARCH_PT_FLAG_USER      ((0x1UL << 2) | (0x1UL << 3))
#define ARCH_PT_FLAG_MAT_CC    (0x1UL << 4)
#define ARCH_PT_FLAG_MAT_WUC   (0x1UL << 5)
#define ARCH_PT_FLAG_GLOBAL    (0x1UL << 6)
#define ARCH_PT_FLAG_HUGE      (0x1UL << 6)
#define ARCH_PT_FLAG_WRITEABLE (0x1UL << 8)
#define ARCH_PT_FLAG_HGLOBAL   (0x1UL << 12)
#define ARCH_PT_FLAG_NX        (0x1UL << 62)

#define ARCH_ADDR_MASK ((uint64_t)0x0000FFFFFFFFF000)

#define ARCH_PT_TABLE_FLAGS 0

#define ARCH_READ_PTE(pte)          ((uint64_t)(pte) & ARCH_ADDR_MASK)
#define ARCH_MAKE_PTE(paddr, flags) (((uint64_t)(paddr) & ARCH_ADDR_MASK) | (flags))
#define ARCH_READ_PTE_FLAG(pte)     ((uint64_t)(pte) & ~ARCH_ADDR_MASK)

#define ARCH_MAKE_HUGE_PTE(paddr, flags)                                                           \
    (((uint64_t)(paddr) & ARCH_ADDR_MASK) | ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_HGLOBAL |            \
     ARCH_PT_FLAG_HUGE | (flags))

#define ARCH_PT_IS_TABLE(x) ((!((x) & (ARCH_PT_FLAG_VALID))) && ((x) != 0))
#define ARCH_PT_IS_LARGE(x)                                                                        \
    (((x) & (ARCH_PT_FLAG_HGLOBAL | ARCH_PT_FLAG_HUGE)) ==                                         \
     (ARCH_PT_FLAG_HGLOBAL | ARCH_PT_FLAG_HUGE))

#define KERNEL_PTE_FLAGS (ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_DIRTY | ARCH_PT_FLAG_WRITEABLE)

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

void     arch_page_setup();
void     unmap_page(page_directory_t *directory, uint64_t vaddr);
void     page_map_to(page_directory_t *directory, uint64_t vaddr, uint64_t paddr, uint64_t flags);
uint64_t get_arch_page_table_flags(uint64_t flags);
uint64_t map_change_attribute(uint64_t *pgdir, uint64_t vaddr, uint64_t flags);
void     free_page_directory(page_directory_t *dir);
page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy);
void              switch_page_directory0(page_directory_t *dir);
