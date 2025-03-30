#pragma once

#define PTE_PRESENT    (0x1 << 0)
#define PTE_WRITEABLE  (0x1 << 1)
#define PTE_USER       (0x1 << 2)
#define PTE_HUGE       (0x1 << 7)
#define PTE_NO_EXECUTE (((uint64_t)0x1) << 63)

#define KERNEL_PTE_FLAGS (PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE)

#define PAGE_SIZE 0x1000

#include "ctype.h"

typedef struct page_table_entry {
    uint64_t value;
} __attribute__((packed)) page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} __attribute__((packed)) page_table_t;

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
/**
 * 多核页切换(不会切换进程的页表, 一般用于进程上下文切换)
 * @param dir 目标页表
 */
void switch_page_directory(page_directory_t *dir);

/**
 * 进程页切换(一般用于内核主动性页表切换)
 * @param dir 目标页表
 */
void switch_process_page_directory(page_directory_t *dir);

/**
 * 分配一页大小的内存
 * @param directory 页目录
 * @param length 长度(字节为单位)
 * @param flags 页标志
 * @return == -1 ? 未分配成功 : 地址
 */
uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags);

page_directory_t *get_current_directory();
void              page_setup();
