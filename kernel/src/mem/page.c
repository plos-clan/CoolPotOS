#include "mem/page.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page_ref.h"
#include "term/kprint.h"

static page_directory_t kernel_dir;

static void unmap_release_table(uint64_t table_phys_addr) {
    if (table_phys_addr)
        address_release(table_phys_addr);
}

page_directory_t get_kernel_page_dir() {
    return kernel_dir;
}

void page_map_range(
    page_directory_t directory, uint64_t addr, uint64_t frame, uint64_t length, uint64_t flags
) {
    for (uint64_t i = 0; i < length; i += PAGE_SIZE) {
        const uint64_t var = addr + i;
        page_map_to(directory, var, frame + i, flags, true);
    }
}

void page_map_range_to_random(
    page_directory_t directory, uint64_t addr, uint64_t length, uint64_t flags
) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = addr + i;
        page_map_to(directory, var, alloc_frames(1), flags, true);
    }
}

bool page_map_to(
    page_directory_t directory, uint64_t addr, uint64_t frame, uint64_t arch_flags, bool force
) {
    assert((addr & 0xfff) == 0);
    assert(frame == (uint64_t)-1 || (frame & 0xfff) == 0);
    uint64_t levels                                        = arch_page_table_levels();
    uint64_t flags                                         = arch_transform_pt_flags(arch_flags);
    uint64_t indexs[ARCH_MAX_PT_LEVEL]                     = { 0 };
    uint64_t *created_parent_tables[ARCH_MAX_PT_LEVEL - 1] = { 0 };
    uint64_t created_parent_indices[ARCH_MAX_PT_LEVEL - 1] = { 0 };
    uint64_t created_table_addrs[ARCH_MAX_PT_LEVEL - 1]    = { 0 };
    size_t created_tables                                  = 0;
    for (uint64_t i = 0; i < levels; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(addr, i + 1);
    }

    for (uint64_t i = 0; i < levels - 1; i++) {
        uint64_t index = indexs[i];
        uint64_t addr  = directory[index];
        if (ARCH_PT_IS_LARGE(addr)) {
            return false;
        }

        if (!ARCH_PT_IS_TABLE(addr)) {
            uint64_t a = alloc_frames(1);
            if (a == 0) {
                return (uint64_t)-1;
            }
            memset(phys_to_virt(a), 0, PAGE_SIZE);
            directory[index]                       = arch_make_page_table_entry(a, flags);
            created_parent_tables[created_tables]  = directory;
            created_parent_indices[created_tables] = index;
            created_table_addrs[created_tables]    = a;
            created_tables++;
        } else {
            if ((flags & ARCH_PT_FLAG_USER) && !(addr & ARCH_PT_FLAG_USER)) {
                uint64_t pa        = ARCH_READ_PTE(addr);
                uint64_t old_flags = ARCH_READ_PTE_FLAG(addr);
                directory[index]   = arch_make_page_table_entry(pa, old_flags | flags);
                arch_flush_tlb(addr);
            }
        }

        directory = (uint64_t *)phys_to_virt(ARCH_READ_PTE(directory[index]));
    }

    uint64_t index       = indexs[levels - 1];
    bool had_old_mapping = (directory[index] & ARCH_PT_FLAG_PRESENT) != 0;
    uint64_t old_frame   = 0;
    if (had_old_mapping) {
        if (!force)
            return true;
        old_frame = ARCH_READ_PTE(directory[index]);
    }

    if (frame == (uint64_t)-1) {
        uint64_t phys = alloc_frames(1);
        if (phys == 0) {
            printk("Cannot allocate frame\n");
            goto rollback_created_tables;
        }
        memset(phys_to_virt(phys), 0, PAGE_SIZE);
        frame = phys;
    } else if (frame && (!had_old_mapping || old_frame != frame) && !address_ref(frame)) {
        goto rollback_created_tables;
    }

    if (had_old_mapping && old_frame && old_frame != frame) {
        address_release(old_frame);
    }

    directory[index] = ARCH_MAKE_PTE(frame, flags);

    arch_flush_tlb(addr);

    return 0;

rollback_created_tables:
    while (created_tables > 0) {
        created_tables--;
        created_parent_tables[created_tables][created_parent_indices[created_tables]] = 0;
        unmap_release_table(created_table_addrs[created_tables]);
    }
    return (uint64_t)-1;
}

void set_kernel_dir(page_directory_t directory) {
    kernel_dir = directory;
}

void page_init() {
    arch_page_table_init();
}
