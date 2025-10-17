#include "page_x64.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "io.h"
#include "lock.h"
#include "krlibc.h"

extern page_directory_t kernel_page_dir;
__attribute__((unused)) uint64_t double_fault_page = 0;
static page_directory_t *current_directory = NULL;

static spin_t page_lock = SPIN_INIT;

static void page_table_clear(page_table_t *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i].value = 0;
    }
}

page_table_t *page_table_create(page_table_entry_t *entry) {
    if (entry->value == 0) {
        uint64_t frame      = alloc_frames(1);
        entry->value        = frame | PTE_PRESENT | PTE_WRITEABLE | PTE_USER;
        page_table_t *table = (page_table_t *)phys_to_virt(entry->value & PTE_FRAME_MASK);
        page_table_clear(table);
        return table;
    }
    page_table_t *table = (page_table_t *)phys_to_virt(entry->value & PTE_FRAME_MASK);
    return table;
}

void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags) {
    uint64_t l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t l1_index = (((addr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table = page_table_create(&(l4_table->entries[l4_index]));
    page_table_t *l2_table = page_table_create(&(l3_table->entries[l3_index]));
    page_table_t *l1_table = page_table_create(&(l2_table->entries[l2_index]));

    l1_table->entries[l1_index].value = (frame & PTE_FRAME_MASK) | flags;

    flush_tlb(addr);
}

void unmap_page(page_directory_t *directory, uint64_t vaddr) {
    uint64_t l4_index = (((vaddr >> 39)) & 0x1FF);
    uint64_t l3_index = (((vaddr >> 30)) & 0x1FF);
    uint64_t l2_index = (((vaddr >> 21)) & 0x1FF);
    uint64_t l1_index = (((vaddr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table = phys_to_virt((&(l4_table->entries[l4_index]))->value & PTE_FRAME_MASK);
    if (l3_table == NULL) return;
    page_table_t *l2_table = phys_to_virt((&(l3_table->entries[l3_index]))->value & PTE_FRAME_MASK);
    if (l2_table == NULL) return;
    page_table_t *l1_table = phys_to_virt((&(l2_table->entries[l2_index]))->value & PTE_FRAME_MASK);
    if (l1_table == NULL) return;

    free_frame(l1_table->entries[l1_index].value & PTE_FRAME_MASK);
    l1_table->entries[l1_index].value = 0;
    flush_tlb(vaddr);
}


static page_table_t *copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy,
                                               bool kernel_space) {
    if (source_table == NULL) return NULL;
    if (level == 0) {
        if (kernel_space) {
            // If we are copying the kernel space, we can just return the same table
            return source_table;
        }

        uint64_t frame = alloc_frames(1);
        not_null_assert((void *)frame, "copy page error");
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        memcpy(new_page_table, source_table->entries, PAGE_SIZE);
        return new_page_table;
    }

    uint64_t phy_frame = alloc_frames(1);
    not_null_assert((void *)phy_frame, "copy page error");
    page_table_t *new_table = phys_to_virt(phy_frame);
    for (uint64_t i = 0; i < (all_copy ? 512 : (level == 4 ? 256 : 512)); i++) {
        if (source_table->entries[i].value & PTE_HUGE) {
            new_table->entries[i].value = source_table->entries[i].value;
            continue;
        }

        page_table_t *source_page_table_next =
            phys_to_virt(source_table->entries[i].value & PTE_FRAME_MASK);
        page_table_t *new_page_table = copy_page_table_recursive(
            source_page_table_next, level - 1, all_copy, level != 4 ? kernel_space : i >= 256);
        new_table->entries[i].value = virt_to_phys(new_page_table) |
                                      (source_table->entries[i].value & 0xFF000000000FFF);
    }
    return new_table;
}

page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy) {
    spin_lock(page_lock);
    page_directory_t *new_directory = malloc(sizeof(page_directory_t));
    if (new_directory == NULL) return NULL;
    new_directory->table = copy_page_table_recursive(dir->table, 4, all_copy, false);
    if (!all_copy)
        memcpy((uint64_t *)new_directory->table + 256, (uint64_t *)dir->table + 256, PAGE_SIZE / 2);
    spin_unlock(page_lock);
    return new_directory;
}

void switch_page_directory(page_directory_t *dir) {
//    if (current_cpu->ready) {
//        current_cpu->directory = dir;
//    } else {
        current_directory = dir;
//    }
    page_table_t *physical_table = (page_table_t*)virt_to_phys(dir->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
}

void arch_page_setup_l2() {
    page_directory_t *new_directory = clone_page_directory(&kernel_page_dir, true);
    kernel_page_dir.table           = new_directory->table;
    free(new_directory);
    switch_page_directory(&kernel_page_dir);
    double_fault_page = get_cr3();
    current_directory = &kernel_page_dir;
}

void arch_page_setup(){
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(get_cr3());
    kernel_page_dir                 = (page_directory_t){.table = kernel_page_table};
    double_fault_page               = get_cr3();
}
