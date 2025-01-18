#include "page.h"
#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "frame.h"

page_directory_t kernel_page_dir;

__IRQHANDLER static void page_fault_handle(interrupt_frame_t *frame,uint64_t error_code) {
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    kerror("Page fault, virtual address 0x%x", faulting_address);
    kerror("Error code: %s\n", !(error_code & 0x1) ? "Page not present" :
                                error_code & 0x2 ? "Write error" :
                                error_code & 0x4 ? "User mode" :
                                error_code & 0x8 ? "Reserved bits set" :
                                error_code & 0x10 ? "Decode address" : "Unknown");
    cpu_hlt;
}

void page_table_clear(page_table_t *table){
    for(int i = 0;i < 512;i++){
        table->entries[i].value = 0;
    }
}

page_table_t *page_table_create(page_table_entry_t *entry){
    if(entry->value == (uint64_t)NULL) {
        uint64_t frame = alloc_frames(1);
        entry->value = frame | PTE_PRESENT | PTE_WRITEABLE | PTE_USER;
        page_table_t *table = (page_table_t *) phys_to_virt(entry->value & 0xFFFFFFFFFFFFF000);
        page_table_clear(table);
        return table;
    }
    page_table_t *table = (page_table_t *) phys_to_virt(entry->value & 0xFFFFFFFFFFFFF000); //获取页帧物理基址并转换成虚拟地址
    return table;
}

page_directory_t *get_kernel_pagedir(){
    return &kernel_page_dir;
}

void page_map_to(page_directory_t *directory,uint64_t addr,uint64_t frame,uint64_t flags){
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

void page_map_range_to(page_directory_t *directory,uint64_t frame,uint64_t length,uint64_t flags){
    for(uint64_t i = 0;i < length;i += 0x1000){
        uint64_t var = (uint64_t)phys_to_virt(frame + i);
        page_map_to(directory,var,frame + i,flags);
    }
}

void page_setup() {
    page_table_t *kernel_page_table = (page_table_t *) phys_to_virt(get_cr3());
    kernel_page_dir = (page_directory_t){.table = kernel_page_table};
    register_interrupt_handler(14, (void *) page_fault_handle, 0, 0x8E);
    kinfo("Kernel page table in 0x%p", kernel_page_table);
}
