#include "page.h"
#include "alloc.h"
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

ticketlock page_lock;

page_directory_t  kernel_page_dir;
page_directory_t *current_directory = NULL;

static bool is_huge_page(page_table_entry_t *entry) {
    return (((uint64_t)entry) & PTE_HUGE) != 0;
}

__IRQHANDLER static void page_fault_handle(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    init_print_lock();
    disable_scheduler();
    switch_page_directory(get_kernel_pagedir());
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    logkf("Page fault, virtual address 0x%x %p\n", faulting_address, frame->rip);
    if (get_current_task() != NULL) {
        logkf("Current process PID: %d:%s (%s)\n", get_current_task()->pid,
              get_current_task()->name,
              get_current_task()->parent_group->name);
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
               get_current_task()->name,get_current_task()->parent_group->name, get_current_cpuid());
    }
    print_register(frame);
    terminal_flush();
    update_terminal();
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
        page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0xFFFFFFFFFFFFF000);
        page_table_clear(table);
        return table;
    }
    page_table_t *table = (page_table_t *)phys_to_virt(
        entry->value & 0xFFFFFFFFFFFFF000); // 获取页帧物理基址并转换成虚拟地址
    return table;
}

page_directory_t *get_kernel_pagedir() {
    return &kernel_page_dir;
}

page_directory_t *get_current_directory() {
    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    return cpu != NULL ? cpu->directory : current_directory;
}

static void copy_page_table_recursive(page_table_t *source_table, page_table_t *new_table, int level) {
    if (level == 0) {
        for (int i = 0; i < 512; i++) {
            new_table->entries[i].value = source_table->entries[i].value;
        }
        return;
    }

    for (int i = 0; i < 512; i++) {
        if (source_table->entries[i].value == 0) {
            new_table->entries[i].value = 0;
            continue;
        }
        page_table_t *source_next_level =
            (page_table_t *)phys_to_virt(source_table->entries[i].value & 0xFFFFFFFFFFFFF000);
        page_table_t *new_next_level = page_table_create(&(new_table->entries[i]));
        new_table->entries[i].value =
            (uint64_t)new_next_level | (source_table->entries[i].value & 0xFFF);

        copy_page_table_recursive(source_next_level, new_next_level, level - 1);
    }
}

page_directory_t *clone_directory(page_directory_t *src) {
    ticket_lock(&page_lock);
    page_directory_t *new_directory = malloc(sizeof(page_directory_t));
    new_directory->table            = malloc(sizeof(page_table_t));
    copy_page_table_recursive(src->table, new_directory->table, 3);
    ticket_unlock(&page_lock);
    return new_directory;
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

    l1_table->entries[l1_index].value = (frame & 0xFFFFFFFFFFFFF000) | flags;

    flush_tlb(addr);
}

void switch_page_directory(page_directory_t *dir) {
    smp_cpu_t *cpu = get_cpu_smp(get_current_cpuid());
    if(cpu != NULL){
        cpu->directory = dir;
    } else current_directory = dir;
    page_table_t *physical_table = virt_to_phys((uint64_t)dir->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
}

void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags){
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
    register_interrupt_handler(14, (void *)page_fault_handle, 0, 0x8E);
    current_directory = &kernel_page_dir;
    kinfo("Kernel page table in 0x%p", kernel_page_table);
}
