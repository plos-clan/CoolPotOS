#include "io.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/page_ref.h"
#include "term/kprint.h"

uint64_t arch_transform_pt_flags(uint64_t flags) {
    uint64_t pt_flags = 0;

    if ((flags & PAGE_TABLE_FLAG_EXEC) == 0) {
        pt_flags |= ARCH_PT_FLAG_NX;
    }
    if ((flags & PAGE_TABLE_FLAG_WRITE) != 0) {
        pt_flags |= ARCH_PT_FLAG_WRITEABLE;
    }
    if ((flags & PAGE_TABLE_FLAG_USER) != 0) {
        pt_flags |= ARCH_PT_FLAG_USER;
    }
    if ((flags & PAGE_TABLE_FLAG_UNCACHEABLE) != 0) {
        pt_flags |= ARCH_PT_FLAG_PCD | ARCH_PT_FLAG_PWT;
    }

    return pt_flags;
}

uint64_t arch_page_table_levels() {
    return 4;
}

void arch_set_current_directory(page_directory_t *dir) {
    uint64_t current_directory = (uint64_t)dir;
    __asm__ volatile("movq %0, %%cr3" ::"r"(current_directory) : "memory");
}

void arch_flush_tlb(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" ::"r"(PADDING_DOWN(addr, PAGE_SIZE)) : "memory");
}

void arch_flush_tlb_all() {
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3)::"memory");
    __asm__ volatile("movq %0, %%cr3" ::"r"(cr3) : "memory");
}

uint64_t arch_make_page_table_entry(uint64_t paddr, uint64_t flags) {
    return ARCH_MAKE_PDE(paddr, ARCH_PT_TABLE_FLAGS | (flags & ARCH_PT_FLAG_USER));
}

void arch_page_table_init() {
    page_directory_t kernel_page_table = phys_to_virt(get_cr3());
    set_kernel_dir(kernel_page_table);
}
