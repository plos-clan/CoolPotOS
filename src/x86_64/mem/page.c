#include "page.h"
#include "frame.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pcb.h"
#include "smp.h"
#include "terminal.h"

spin_t page_lock;

page_directory_t  kernel_page_dir;
page_directory_t *current_directory = NULL;

uint64_t double_fault_page = 0;

static bool is_huge_page(page_table_entry_t *entry) {
    return (((uint64_t)entry->value) & PTE_HUGE) != 0;
}

__IRQHANDLER static void page_fault_handle(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    init_print_lock();
    disable_scheduler();
    terminal_close_flush();
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    if (get_current_task() != NULL) {
        pcb_t current_proc = get_current_task()->parent_group;
        logkf("#PF Current process PID: %d:%s (%s)\n", get_current_task()->pid,
              get_current_task()->name, current_proc->name);

        if (current_proc->task_level == TASK_APPLICATION_LEVEL) {
            mm_virtual_page_t *virt_page = NULL;
            spin_lock(current_proc->virt_queue->lock);
            queue_foreach(current_proc->virt_queue, node) {
                mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
                if (faulting_address >= vpage->start &&
                    faulting_address < vpage->start + vpage->count * PAGE_SIZE) {
                    virt_page = vpage;
                    break;
                }
            }
            spin_unlock(current_proc->virt_queue->lock);
            if (virt_page == NULL) {
                kill_proc(get_current_task()->parent_group, -1);
            } else {
                // 实际分配预分配的地址
                for (size_t i = 0; i < virt_page->count; i++) {
                    uint64_t page_addr = virt_page->start + i * PAGE_SIZE;

                    //        if (flags & MAP_FIXED) {
                    //TODO 检查地址是否已被映射, 并分配空闲地址
                    //        } else {
                    uint64_t phys = alloc_frames(1);
                    page_map_to(get_current_directory(), page_addr, phys, virt_page->pte_flags);
                    //        }
                    memset((void *)page_addr, 0, PAGE_SIZE);
                }
                queue_remove_at(current_proc->virt_queue, virt_page->index);
                free(virt_page);
            }
        }
        terminal_open_flush();
        enable_scheduler();
        cpu_hlt;
    } else {
        logkf("Page fault virtual address 0x%x %p\n", faulting_address, frame->rip);
        logkf("Type: %s\n", !(error_code & 0x1) ? "NotPresent"
                            : error_code & 0x2  ? "WriteError"
                            : error_code & 0x4  ? "UserMode"
                            : error_code & 0x8  ? "ReservedBitsSet"
                            : error_code & 0x10 ? "DecodeAddress"
                                                : "Unknown");
    }

    printk("\n");
    printk(
        "\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(PageFault%s:0x%p)<\033[0m\n",
        !(error_code & 0x1) ? "NotPresent"
        : error_code & 0x2  ? "WriteError"
        : error_code & 0x4  ? "UserMode"
        : error_code & 0x8  ? "ReservedBitsSet"
        : error_code & 0x10 ? "DecodeAddress"
                            : "Unknown",
        faulting_address);
    if (get_current_task() != NULL) {
        printk("Current process PID: %d:%s (%s) at CPU%d\n", get_current_task()->pid,
               get_current_task()->name, get_current_task()->parent_group->name, cpu->id);
    }
    print_register(frame);
    terminal_flush();
    update_terminal();
    terminal_open_flush();
    cpu_hlt;
}

void page_table_clear(page_table_t *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i].value = 0;
    }
}

page_table_t *page_table_create(page_table_entry_t *entry) {
    if (entry->value == (uint64_t)NULL) {
        uint64_t frame      = alloc_frames(1);
        entry->value        = frame | PTE_PRESENT | PTE_WRITEABLE | PTE_USER;
        page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0x000fffffffff000);
        page_table_clear(table);
        return table;
    }
    page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0x000fffffffff000);
    return table;
}

page_directory_t *get_kernel_pagedir() {
    return &kernel_page_dir;
}

page_directory_t *get_current_directory() {
    return cpu->ready ? cpu->directory : current_directory;
}

static void copy_page_table_recursive(page_table_t *source_table, page_table_t *new_table,
                                      int level) {
    for (int i = 0; i < 512; i++) {
        page_table_entry_t *entry = &source_table->entries[i];

        if (level == 1 || entry->value == 0 || is_huge_page(entry)) {
            new_table->entries[i].value = entry->value;
            continue;
        }

        uint64_t      frame          = alloc_frames(1);
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        new_table->entries[i].value  = frame | (entry->value & 0xFFF);

        page_table_t *source_page_table_next = phys_to_virt(entry->value & 0x000fffffffff000);
        page_table_t *target_page_table_next = new_page_table;

        copy_page_table_recursive(source_page_table_next, target_page_table_next, level - 1);
    }
}

page_directory_t *clone_directory(page_directory_t *src) {
    spin_lock(page_lock);
    page_directory_t *new_directory = malloc(sizeof(page_directory_t));
    if (new_directory == NULL) return NULL;
    uint64_t phy_frame   = alloc_frames(1);
    new_directory->table = phys_to_virt(phy_frame);
    copy_page_table_recursive(src->table, new_directory->table, 4);
    spin_unlock(page_lock);
    return new_directory;
}

static void free_page_table_recursive(page_table_t *table, int level) {
    uint64_t virtual_address  = (uint64_t)table;
    uint64_t physical_address = (uint64_t)virt_to_phys(virtual_address);
    if (level == 0) {
        free_frame(physical_address & 0x000fffffffff000);
        return;
    }

    for (int i = 0; i < 512; i++) {
        page_table_entry_t *entry = &table->entries[i];
        if (entry->value == 0 || is_huge_page(entry)) continue;

        if (level == 1) {
            if (entry->value & PTE_PRESENT && entry->value & PTE_WRITEABLE &&
                entry->value & PTE_USER) {
                free_frame(entry->value & 0x000fffffffff000);
            }
        } else {
            free_page_table_recursive(phys_to_virt(entry->value & 0x000fffffffff000), level - 1);
        }
    }
    free_frame(physical_address & 0x000fffffffff000);
}

void free_page_directory(page_directory_t *dir) {
    spin_lock(page_lock);
    free_page_table_recursive(dir->table, 4);
    free_frame((uint64_t)virt_to_phys((uint64_t)dir->table));
    free(dir);
    spin_unlock(page_lock);
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

    l1_table->entries[l1_index].value = (frame & 0x000fffffffff000) | flags;

    flush_tlb(addr);
}

void switch_process_page_directory(page_directory_t *dir) {
    disable_scheduler();
    close_interrupt;
    pcb_t pcb     = get_current_task()->parent_group;
    pcb->page_dir = dir;
    switch_page_directory(dir);
    enable_scheduler();
    open_interrupt;
}

void switch_page_directory(page_directory_t *dir) {
    if (cpu->ready) {
        cpu->directory = dir;
    } else {
        current_directory = dir;
    }
    page_table_t *physical_table = virt_to_phys((uint64_t)dir->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
}

uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags) {
    if (length == 0) return -1;
    size_t   p    = length / PAGE_SIZE;
    uint64_t addr = alloc_frames(p == 0 ? 1 : p);
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, var, flags);
    }
    return addr;
}

void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, alloc_frames(1), flags);
    }
}

void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)phys_to_virt(frame + i);
        page_map_to(directory, var, frame + i, flags);
    }
}

void page_setup() {
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(get_cr3());
    kernel_page_dir                 = (page_directory_t){.table = kernel_page_table};
    double_fault_page               = get_cr3();
    register_interrupt_handler(14, (void *)page_fault_handle, 0, 0x8E);
    current_directory = &kernel_page_dir;
}
