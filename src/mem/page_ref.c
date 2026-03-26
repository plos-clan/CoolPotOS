#include "mem/page_ref.h"
#include "krlibc.h"
#include "mem/page.h"
#include "mem/bitmap.h"
#include "mem/buddy.h"

page_t *page_maps;

extern uint64_t memory_size;
extern Bitmap usable_regions;

extern void *early_alloc(size_t size);

void page_ref_init() {
    uint64_t page_maps_size = memory_size / PAGE_SIZE * sizeof(page_t);
    page_maps               = early_alloc(page_maps_size);
    asserts(page_maps, "page_init: page_maps is null.");
    memset(page_maps, 0, page_maps_size);
}

page_t *get_page_ref(uint64_t addr) {
    return page_maps + (addr / PAGE_SIZE);
}

int page_refcount_read(page_t *page) {
    if (!page)
        return 0;
    return __atomic_load_n(&page->refcount, __ATOMIC_ACQUIRE);
}

void page_ref(page_t *page) {
    if (page)
        __atomic_add_fetch(&page->refcount, 1, __ATOMIC_ACQ_REL);
}

bool page_try_ref(page_t *page) {
    if (!page)
        return false;

    int refs = __atomic_load_n(&page->refcount, __ATOMIC_ACQUIRE);
    while (refs > 0) {
        int new_refs = refs + 1;
        if (__atomic_compare_exchange_n(
                &page->refcount, &refs, new_refs, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE
            )) {
            return true;
        }
    }

    return false;
}

int page_unref(page_t *page) {
    if (!page)
        return -1;

    int refs = __atomic_load_n(&page->refcount, __ATOMIC_ACQUIRE);
    while (refs > 0) {
        int new_refs = refs - 1;
        if (__atomic_compare_exchange_n(
                &page->refcount, &refs, new_refs, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE
            )) {
            return new_refs;
        }
    }

    return -1;
}

bool page_try_release_last(page_t *page) {
    if (!page)
        return false;

    int expected = 1;
    return __atomic_compare_exchange_n(
        &page->refcount, &expected, 0, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE
    );
}

bool page_can_free(page_t *page) {
    return page_refcount_read(page) <= 0;
}

bool address_ref(uint64_t addr) {
    if (!address_is_managed(addr))
        return true;

    return page_try_ref(get_page_ref(addr));
}
void address_unref(uint64_t addr) {
    if (address_is_managed(addr))
        page_unref(get_page_ref(addr));
}

bool address_can_free(uint64_t addr) {
    return address_is_managed(addr) ? page_can_free(get_page_ref(addr)) : false;
}

bool address_is_managed(uint64_t addr) {
    if (!page_maps || addr >= memory_size)
        return false;

    size_t page_index = addr / PAGE_SIZE;
    if (page_index >= usable_regions.length)
        return false;

    return bitmap_get(&usable_regions, page_index);
}

void address_release(uint64_t addr) {
    if (!address_is_managed(addr))
        return;

    page_t *page = get_page_ref(addr);
    if (!page)
        return;

    int refs = page_unref(page);
    if (refs > 0 || refs < 0)
        return;

    free_frames_released(addr, 1);
}
