#pragma once

#include "types.h"

typedef struct page {
    int refcount;
    // TODO
} page_t;

extern page_t *page_maps;

void page_ref_init();

page_t *get_page_ref(uint64_t addr);

int page_refcount_read(page_t *page);
void page_ref(page_t *page);
bool page_try_ref(page_t *page);
int page_unref(page_t *page);
bool page_try_release_last(page_t *page);
bool page_can_free(page_t *page);

bool address_ref(uint64_t addr);
void address_unref(uint64_t addr);
bool address_can_free(uint64_t addr);
bool address_is_managed(uint64_t addr);
void address_release(uint64_t addr);
