#pragma once

#define PTE_PRESENT    (0x1 << 0)
#define PTE_WRITEABLE  (0x1 << 1)
#define PTE_USER       (0x1 << 2)
#define PTE_HUGE       (0x1 << 3)
#define PTE_NO_EXECUTE (((uint64_t)0x1) << 63)

#define KERNEL_PTE_FLAGS (PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE)

#define PAGE_SIZE 0x1000

#include "ctype.h"

typedef struct page_table_entry {
    uint64_t value;
} page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} page_table_t;

typedef struct page_directory {
    page_table_t *table;
} page_directory_t;

page_directory_t *get_kernel_pagedir();
void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);
void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags);
void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags);
page_directory_t *clone_directory(page_directory_t *src);
void              switch_page_directory(page_directory_t *dir);
page_directory_t *get_current_directory();
void              page_setup();
