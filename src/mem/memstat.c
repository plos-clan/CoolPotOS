#include "mem/memstat.h"
#include "boot.h"
#include "mem/buddy.h"
#include "mem/page.h"

static uint64_t memstat_cap_bytes(void) {
    return get_total_frames() * PAGE_SIZE;
}

static uint64_t memstat_align_up(const uint64_t value) {
    return value + PAGE_SIZE - 1 & ~(PAGE_SIZE - 1);
}

static uint64_t memstat_align_down(const uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

static void memstat_sum_zone_pages(size_t *managed_pages, size_t *free_pages) {
    size_t managed = 0;
    size_t free    = 0;

    for (int i = 0; i < nr_zones; i++) {
        const zone_t *zone = zones[i];
        if (zone == NULL) {
            continue;
        }
        managed += zone->managed_pages;
        free += zone->free_pages;
    }

    if (managed_pages != NULL) {
        *managed_pages = managed;
    }
    if (free_pages != NULL) {
        *free_pages = free;
    }
}

static uint64_t memstat_bad_frames(void) {
    const boot_memory_map_t *memory_map = boot_get_memory_map();
    if (memory_map == NULL) {
        return 0;
    }

    const uint64_t cap = memstat_cap_bytes();
    uint64_t bad_pages = 0;

    for (size_t i = 0; i < memory_map->entry_count; i++) {
        const boot_memory_map_entry_t *entry = &memory_map->entries[i];
        if (entry->type != BOOT_MMAP_BAD) {
            continue;
        }

        const uint64_t start = memstat_align_up(entry->base);
        uint64_t end         = memstat_align_down(entry->base + entry->length);
        if (start >= cap) {
            continue;
        }
        if (end > cap) {
            end = cap;
        }
        if (end <= start) {
            continue;
        }

        bad_pages += (end - start) / PAGE_SIZE;
    }

    return bad_pages;
}

uint64_t get_reserved_memory() {
    const uint64_t total = get_total_frames() * PAGE_SIZE;
    const uint64_t all   = get_all_memory();
    const uint64_t bad   = get_bad_memory();

    if (total <= all + bad) {
        return 0;
    }
    return total - all - bad;
}

uint64_t get_all_memory() {
    return get_origin_frames() * PAGE_SIZE;
}

uint64_t get_available_memory() {
    return get_usable_frames() * PAGE_SIZE;
}

uint64_t get_used_memory() {
    const size_t origin = get_origin_frames();
    const size_t usable = get_usable_frames();
    return (origin > usable ? origin - usable : 0) * PAGE_SIZE;
}

uint64_t get_bad_memory() {
    return memstat_bad_frames() * PAGE_SIZE;
}

size_t get_total_frames() {
    size_t total = 0;

    for (int i = 0; i < nr_zones; i++) {
        const zone_t *zone = zones[i];
        if (zone == NULL || zone->zone_end_pfn <= zone->zone_start_pfn) {
            continue;
        }
        total += zone->zone_end_pfn - zone->zone_start_pfn;
    }

    return total;
}

size_t get_origin_frames() {
    size_t managed = 0;
    memstat_sum_zone_pages(&managed, NULL);
    return managed;
}

size_t get_usable_frames() {
    size_t free = 0;
    memstat_sum_zone_pages(NULL, &free);
    return free;
}
