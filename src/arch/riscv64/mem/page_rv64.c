#include "page_rv64.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"

extern page_directory_t kernel_page_dir;
__attribute__((unused)) uint64_t double_fault_page = 0;
static spin_t page_lock                            = SPIN_INIT;

uint64_t get_arch_page_table_flags(uint64_t flags) {
    uint64_t result = ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_ACCESSED | ARCH_PT_FLAG_DIRTY;
    if (flags & ARCH_PT_FLAG_WRITE)
        result |= ARCH_PT_FLAG_WRITE;
    if (flags & ARCH_PT_FLAG_READ)
        result |= ARCH_PT_FLAG_READ;
    if (flags & ARCH_PT_FLAG_USER)
        result |= ARCH_PT_FLAG_USER;
    if (flags & ARCH_PT_FLAG_EXEC)
        result |= ARCH_PT_FLAG_EXEC;
    return result;
}

void flush_tlb(uint64_t vaddr) {
    __asm__ volatile("sfence.vma %0, zero" : : "r"(vaddr) : "memory");
}

void switch_page_directory0(page_directory_t *dir) {
    page_table_t *physical_table = (page_table_t *)virt_to_phys(dir->table);
    uint64_t satp                = MAKE_SATP_PADDR(SATP_MODE_SV48, 0, (uint64_t)physical_table);
    __asm__ volatile("csrw satp, %0" : : "r"(satp) : "memory");
    __asm__ volatile("sfence.vma" : : : "memory");
}

uint64_t arch_virt_to_phys(uint64_t vaddr) {
    if (!vaddr)
        return 0;
    page_directory_t *curr = get_current_directory();
    uint64_t *pgdir        = (uint64_t *)curr->table;

    uint64_t indexs[ARCH_PT_LEVEL];
    for (uint64_t i = 0; i < ARCH_PT_LEVEL; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(vaddr, i + 1);
    }

    for (uint64_t i = 0; i < ARCH_PT_LEVEL - 1; i++) {
        uint64_t index = indexs[i];
        uint64_t addr  = pgdir[index];
        if (ARCH_PT_IS_LARGE(addr)) {
            return (ARCH_READ_PTE(pgdir[index]) & ~PAGE_CALC_PAGE_TABLE_MASK(i + 1))
                   + (vaddr & PAGE_CALC_PAGE_TABLE_MASK(i + 1));
        }
        if (!ARCH_PT_IS_TABLE(addr)) {
            return 0;
        }
        pgdir = (uint64_t *)phys_to_virt(ARCH_READ_PTE(addr));
    }

    uint64_t index = indexs[ARCH_PT_LEVEL - 1];
    return ARCH_READ_PTE(pgdir[index]) + (vaddr & PAGE_CALC_PAGE_TABLE_MASK(ARCH_PT_LEVEL));
}

void page_map_to(page_directory_t *directory, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t *pgdir                = (uint64_t *)directory->table;
    uint64_t indexs[ARCH_PT_LEVEL] = { 0 };
    for (uint64_t i = 0; i < ARCH_PT_LEVEL; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(vaddr, i + 1);
    }

    for (uint64_t i = 0; i < ARCH_PT_LEVEL - 1; i++) {
        uint64_t index = indexs[i];
        uint64_t addr  = pgdir[index];
        if (ARCH_PT_IS_LARGE(addr)) {
            return;
        }

        if (!ARCH_PT_IS_TABLE(addr)) {
            uint64_t a = alloc_frames(1);
            not_null_assert((void *)a, "page_map_to alloc frame null.");
            memset((uint64_t *)phys_to_virt(a), 0, PAGE_SIZE);
            pgdir[index] = ARCH_MAKE_PTE(a, ARCH_PT_TABLE_FLAGS);
        }
        pgdir = (uint64_t *)phys_to_virt(ARCH_READ_PTE(pgdir[index]));
    }

    uint64_t index = indexs[ARCH_PT_LEVEL - 1];
    if (pgdir[index] & ARCH_PT_FLAG_VALID) {
        // TODO force
    }

    pgdir[index] = ARCH_MAKE_PTE(paddr, flags);
    flush_tlb(vaddr);
}

void unmap_page(page_directory_t *directory, uint64_t vaddr) {
    uint64_t *pgdir                = (uint64_t *)directory->table;
    uint64_t indexs[ARCH_PT_LEVEL] = { 0 };
    for (uint64_t i = 0; i < ARCH_PT_LEVEL; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(vaddr, i + 1);
    }

    page_table_t *tables[4];
    tables[0] = (page_table_t *)pgdir;

    for (int i = 0; i < 3; i++) {
        page_table_entry_t *entry = &tables[i]->entries[indexs[i]];
        if ((entry->value & ARCH_PT_FLAG_VALID) == 0) {
            return;
        }
        uint64_t next_pa = entry->value & ARCH_ADDR_MASK;
        tables[i + 1]    = (page_table_t *)phys_to_virt(next_pa);
    }

    page_table_entry_t *pte = &tables[3]->entries[indexs[3]];
    if ((pte->value & ARCH_PT_FLAG_VALID) == 0) {
        return;
    }

    uint64_t paddr = pte->value & ARCH_ADDR_MASK;
    if (paddr != 0) {
        free_frames(paddr, 1);
    }

    pte->value = 0;
    flush_tlb(vaddr);

    for (int level = 3; level >= 1; level--) {
        page_table_t *current = tables[level];
        bool empty            = true;

        for (int i = 0; i < 512; i++) {
            if (current->entries[i].value != 0) {
                empty = false;
                break;
            }
        }

        if (empty) {
            uint64_t table_pa = virt_to_phys(current);
            free_frame(table_pa);
            page_table_entry_t *parent_entry = &tables[level - 1]->entries[indexs[level - 1]];
            parent_entry->value              = 0;
        } else {
            break;
        }
    }
}

uint64_t map_change_attribute(uint64_t *pgdir, uint64_t vaddr, uint64_t flags) {
    if (vaddr < PAGE_SIZE)
        return -1;
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
        if (!ARCH_PT_IS_TABLE(addr)) {
            return 0;
        }
        pgdir = (uint64_t *)phys_to_virt(addr & (~PAGE_CALC_PAGE_TABLE_MASK(4)));
    }

    uint64_t index = indexs[4 - 1];

    pgdir[index] &= ~PAGE_CALC_PAGE_TABLE_MASK(4);
    pgdir[index] |= flags;

    flush_tlb(vaddr);
    return 0;
}

static void free_page_table_recursive(page_table_t *table, int level) {
    if (table == NULL)
        return;
    if (level == 0) {
        free_frame(virt_to_phys(table));
        return;
    }
    int num_entries = (level == 4) ? 256 : 512;
    for (int i = 0; i < num_entries; i++) {
        uint64_t entry_val = table->entries[i].value;
        if ((entry_val & ARCH_PT_FLAG_VALID) == 0) {
            continue;
        }
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

static page_table_t *
copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy, bool kernel_space) {
    if (source_table == NULL)
        return NULL;
    if (level == 0) {
        if (kernel_space) {
            return source_table;
        }

        uint64_t frame               = alloc_frames(1);
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        memcpy(new_page_table, source_table, PAGE_SIZE);
        return new_page_table;
    }

    uint64_t phy_frame      = alloc_frames(1);
    page_table_t *new_table = (page_table_t *)phys_to_virt(phy_frame);
    for (uint64_t i = 0; i < (all_copy ? 512 : (level == ARCH_PT_LEVEL ? 256 : 512)); i++) {
        if (ARCH_PT_IS_LARGE(source_table->entries[i].value) && level != 1) {
            new_table->entries[i].value = source_table->entries[i].value;
            continue;
        }

        page_table_t *source_page_table_next =
            (page_table_t *)phys_to_virt(ARCH_READ_PTE(source_table->entries[i].value));
        page_table_t *new_page_table = copy_page_table_recursive(
            source_page_table_next,
            level - 1,
            all_copy,
            level != ARCH_PT_LEVEL ? kernel_space : i >= 256
        );
        new_table->entries[i].value = ARCH_MAKE_PTE(
            (uint64_t)virt_to_phys(new_page_table),
            ARCH_READ_PTE_FLAG(source_table->entries[i].value)
        );
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
        memcpy(
            (uint8_t *)new_directory->table + 256 * sizeof(page_table_entry_t),
            (uint8_t *)dir->table + 256 * sizeof(page_table_entry_t),
            256 * sizeof(page_table_entry_t)
        );
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
    uint64_t satp                   = read_satp();
    uint64_t root_ppn               = satp & SATP_PPN_MASK;
    uint64_t page_table_base        = root_ppn << 12;
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(page_table_base);
    kernel_page_dir                 = (page_directory_t){ .table = kernel_page_table };
    double_fault_page               = page_table_base;
}
