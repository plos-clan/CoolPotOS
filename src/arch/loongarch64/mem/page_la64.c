#include "page_la64.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"

uint64_t get_arch_page_table_flags(uint64_t flags) {
    return flags;
}

void page_map_to(page_directory_t *directory, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    // TODO
}

void switch_page_directory0(page_directory_t *dir) {
}

uint64_t map_change_attribute(uint64_t *pgdir, uint64_t vaddr, uint64_t flags) {
}

void free_page_directory(page_directory_t *dir) {
}

page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy) {
}

void unmap_page(page_directory_t *directory, uint64_t vaddr) {
}

void arch_page_setup() {
}

uint64_t arch_virt_to_phys(uint64_t vaddr) {
}
