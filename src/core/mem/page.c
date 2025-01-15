#include "page.h"
#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"
#include "hhdm.h"
#include "io.h"

page_directory_t kernel_page_dir;

static void page_fault_handle() {
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    kerror("Page fault, virtual address 0x%x\n", faulting_address);
    cpu_hlt;
}

void page_setup() {
    page_table_t *kernel_page_table = (page_table_t *) phys_to_virt(get_cr3());
    kernel_page_dir = (page_directory_t){.table = kernel_page_table};
    register_interrupt_handler(14, (void *) page_fault_handle, 0, 0x8E);
    kinfo("Kernel page table in 0x%p", kernel_page_table);
}
