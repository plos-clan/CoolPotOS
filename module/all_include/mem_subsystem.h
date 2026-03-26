#pragma once

#define PAGE_SIZE 4096

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)
#define PADDING_REQ(size, to)  ((size + (to) - 1) & ~((to) - 1) / (to))

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

uint64_t alloc_frames(size_t count);
void free_frames(uint64_t addr, size_t count);
void free_frame(uint64_t addr);

void *driver_phys_to_virt(uint64_t phys_addr);
uint64_t driver_virt_to_phys(void *virt_addr);
uint64_t arch_virt_to_phys(uint64_t va);
void *phys_to_virt(uint64_t phys_addr);

page_directory_t *get_kernel_pagedir();
uint64_t get_kernel_pte_flags();

void page_map_range(
    page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length, uint64_t flags
);
void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);
