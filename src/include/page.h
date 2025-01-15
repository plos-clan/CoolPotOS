#pragma once

#include "ctype.h"

typedef struct page_table_entry {
    uint64_t present: 1;
    uint64_t writable: 1;
    uint64_t user: 1;
    uint64_t write_through: 1;
    uint64_t cache_disabled: 1;
    uint64_t accessed: 1;
    uint64_t dirty: 1;
    uint64_t huge_page: 1;
    uint64_t global: 1;
    uint64_t available: 3;
    uint64_t frame: 40;
    uint64_t reserved: 11;
    uint64_t no_execute: 1;
} page_table_entry_t;

typedef struct {
    uint64_t entries[512];
} page_table_t;

typedef struct page_directory {
    page_table_t *table;
} page_directory_t;

void page_setup();
