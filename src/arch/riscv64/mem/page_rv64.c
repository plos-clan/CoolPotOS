#include "page_rv64.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "term/klog.h"

extern page_directory_t          kernel_page_dir;
__attribute__((unused)) uint64_t double_fault_page = 0;
static spin_t                    page_lock         = SPIN_INIT;

uint64_t get_arch_page_table_flags(uint64_t flags) {
    uint64_t result = ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_ACCESSED | ARCH_PT_FLAG_DIRTY;
    if(flags & ARCH_PT_FLAG_WRITE) result |= ARCH_PT_FLAG_WRITE;
    if(flags & ARCH_PT_FLAG_READ) result |= ARCH_PT_FLAG_READ;
    if(flags & ARCH_PT_FLAG_USER) result |= ARCH_PT_FLAG_USER;
    if(flags & ARCH_PT_FLAG_EXEC) result |= ARCH_PT_FLAG_EXEC;
    return result;
}

void flush_tlb(uint64_t vaddr) {
    __asm__ volatile("sfence.vma %0, zero" : : "r"(vaddr) : "memory");
}

void switch_page_directory0(page_directory_t *dir) {
    page_table_t *physical_table = (page_table_t *)virt_to_phys(dir->table);
    uint64_t      satp           = MAKE_SATP_PADDR(SATP_MODE_SV48, 0, (uint64_t)physical_table);
    __asm__ volatile("csrw satp, %0" : : "r"(satp) : "memory");
    __asm__ volatile("sfence.vma" : : : "memory");
}

static void page_table_clear(page_table_t *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i].value = 0;
    }
}

uint64_t arch_virt_to_phys(uint64_t va) {
    return 0; // TODO
}

void page_map_to(page_directory_t *directory, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    page_table_t *pgdir      = directory->table;
    uint64_t      indices[4] = {
        (vaddr >> (12 + 9 * 3)) & 0x1FF, // Level 3 (PGD)
        (vaddr >> (12 + 9 * 2)) & 0x1FF, // Level 2 (PUD)
        (vaddr >> (12 + 9 * 1)) & 0x1FF, // Level 1 (PMD)
        (vaddr >> 12) & 0x1FF            // Level 0 (PTE)
    };

    page_table_t *tables[4];
    tables[0] = pgdir;

    for (int i = 0; i < 3; i++) {
        page_table_entry_t *entry = &tables[i]->entries[indices[i]];
        if ((entry->value & ARCH_PT_FLAG_VALID) == 0) {
            uint64_t frame = alloc_frames(1);
            if (frame == 0) { logkf("page_map_to: failed to allocate page table frame"); }
            page_table_t *new_table = (page_table_t *)phys_to_virt(frame);
            page_table_clear(new_table);
            entry->value = frame | ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_WRITE;
        }
        uint64_t next_table_pa = entry->value & ARCH_ADDR_MASK;
        tables[i + 1]          = (page_table_t *)phys_to_virt(next_table_pa);
    }

    page_table_entry_t *pte = &tables[3]->entries[indices[3]];
    if (pte->value & ARCH_PT_FLAG_VALID) {
        // 已映射：可根据需求处理（覆盖 or 忽略）此处选择直接覆盖
    }

    pte->value = (paddr & ARCH_ADDR_MASK) | flags;
    flush_tlb(vaddr);
}

void unmap_page(page_directory_t *directory, uint64_t vaddr) {
    page_table_t *pgdir      = directory->table;
    uint64_t      indices[4] = {
        (vaddr >> (12 + 9 * 3)) & 0x1FF, // L3 (PGD)
        (vaddr >> (12 + 9 * 2)) & 0x1FF, // L2 (PUD)
        (vaddr >> (12 + 9 * 1)) & 0x1FF, // L1 (PMD)
        (vaddr >> 12) & 0x1FF            // L0 (PTE)
    };

    page_table_t *tables[4];
    tables[0] = pgdir;

    for (int i = 0; i < 3; i++) {
        page_table_entry_t *entry = &tables[i]->entries[indices[i]];
        if ((entry->value & ARCH_PT_FLAG_VALID) == 0) { return; }
        uint64_t next_pa = entry->value & ARCH_ADDR_MASK;
        tables[i + 1]    = (page_table_t *)phys_to_virt(next_pa);
    }

    page_table_entry_t *pte = &tables[3]->entries[indices[3]];
    if ((pte->value & ARCH_PT_FLAG_VALID) == 0) { return; }

    uint64_t paddr = pte->value & ARCH_ADDR_MASK;
    if (paddr != 0) { free_frames(paddr, 1); }

    pte->value = 0;
    flush_tlb(vaddr);

    for (int level = 3; level >= 1; level--) {
        page_table_t *current = tables[level];
        bool          empty   = true;

        for (int i = 0; i < 512; i++) {
            if (current->entries[i].value != 0) {
                empty = false;
                break;
            }
        }

        if (empty) {
            uint64_t table_pa = virt_to_phys(current);
            free_frame(table_pa);
            page_table_entry_t *parent_entry = &tables[level - 1]->entries[indices[level - 1]];
            parent_entry->value              = 0;
        } else {
            break;
        }
    }
}

uint64_t map_change_attribute(uint64_t *pgdir, uint64_t vaddr, uint64_t flags) {
    uint64_t indexs[4];
    for (uint64_t i = 0; i < 4; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(vaddr, i + 1);
    }

    for (uint64_t i = 0; i < 4 - 1; i++) {
        uint64_t index = indexs[i];
        uint64_t addr  = pgdir[index];
        if (ARCH_PT_IS_LARGE(addr)) {
            pgdir[index] &= ~PAGE_CALC_PAGE_TABLE_MASK(4);
            pgdir[index] |= flags;
        }
        if (!ARCH_PT_IS_TABLE(addr)) { return 0; }
        pgdir = (uint64_t *)phys_to_virt(addr & (~PAGE_CALC_PAGE_TABLE_MASK(4)));
    }

    uint64_t index = indexs[4 - 1];

    pgdir[index] &= ~PAGE_CALC_PAGE_TABLE_MASK(4);
    pgdir[index] |= flags;

    flush_tlb(vaddr);
    return 0;
}

static void free_page_table_recursive(page_table_t *table, int level) {
    if (table == NULL) return;
    if (level == 0) {
        free_frame(virt_to_phys(table));
        return;
    }
    int num_entries = (level == 4) ? 256 : 512;
    for (int i = 0; i < num_entries; i++) {
        uint64_t entry_val = table->entries[i].value;
        if ((entry_val & ARCH_PT_FLAG_VALID) == 0) { continue; }
        page_table_t *next_table = (page_table_t *)phys_to_virt(entry_val & ARCH_ADDR_MASK);
        free_page_table_recursive(next_table, level - 1);
    }
    free_frame(virt_to_phys(table));
}

void free_page_directory(page_directory_t *dir) {
    spin_lock(page_lock);
    free_page_table_recursive(dir->table, 4);
    free(dir);
    spin_unlock(page_lock);
}

static page_table_t *copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy,
                                               bool kernel_space) {
    if (source_table == NULL) return NULL;

    if (level == 0) {
        if (kernel_space) { return source_table; }

        uint64_t frame = alloc_frames(1);
        not_null_assert((void *)frame, "copy page error");
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        memcpy(new_page_table, source_table, sizeof(page_table_t)); // 整个结构体复制
        return new_page_table;
    }

    uint64_t phy_frame = alloc_frames(1);
    not_null_assert((void *)phy_frame, "copy page error");
    page_table_t *new_table = (page_table_t *)phys_to_virt(phy_frame);
    memset(new_table, 0, sizeof(page_table_t));
    uint64_t num_entries = all_copy ? 512 : (level == 4 ? 256 : 512);

    for (uint64_t i = 0; i < num_entries; i++) {
        uint64_t entry_val = source_table->entries[i].value;
        if ((entry_val & ARCH_PT_FLAG_VALID) == 0) {
            new_table->entries[i].value = 0;
            continue;
        }
        uint64_t      next_pa        = entry_val & ARCH_ADDR_MASK;
        page_table_t *source_next    = (page_table_t *)phys_to_virt(next_pa);
        bool          next_is_kernel = (level == 4) ? (i >= 256) : kernel_space;
        page_table_t *new_next =
            copy_page_table_recursive(source_next, level - 1, all_copy, next_is_kernel);
        uint64_t new_pa             = virt_to_phys(new_next);
        uint64_t flags              = entry_val & ~ARCH_ADDR_MASK;
        new_table->entries[i].value = new_pa | flags;
    }

    return new_table;
}

page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy) {
    spin_lock(page_lock);

    page_directory_t *new_directory = malloc(sizeof(page_directory_t));
    if (new_directory == NULL) {
        spin_unlock(page_lock);
        return NULL;
    }

    new_directory->table = copy_page_table_recursive(dir->table, 4, all_copy, false);

    if (!all_copy) {
        memcpy((uint8_t *)new_directory->table + 256 * sizeof(page_table_entry_t),
               (uint8_t *)dir->table + 256 * sizeof(page_table_entry_t),
               256 * sizeof(page_table_entry_t));
    }

    spin_unlock(page_lock);
    return new_directory;
}

void arch_page_setup_l2() {
    page_directory_t *new_directory = clone_page_directory(&kernel_page_dir, true);
    kernel_page_dir.table           = new_directory->table;
    free(new_directory);
    switch_page_directory0(&kernel_page_dir);

    uint64_t satp     = read_satp();
    uint64_t root_ppn = satp & SATP_PPN_MASK;
    double_fault_page = root_ppn << 12;
}

void arch_page_setup() {
    uint64_t      satp              = read_satp();
    uint64_t      root_ppn          = satp & SATP_PPN_MASK;
    uint64_t      page_table_base   = root_ppn << 12;
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(page_table_base);
    kernel_page_dir                 = (page_directory_t){.table = kernel_page_table};
    double_fault_page               = page_table_base;
}
