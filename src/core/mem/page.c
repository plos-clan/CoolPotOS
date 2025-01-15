#include "page.h"
#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"

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

void page_setup() {
    page_table_t *kernel_page_table = (page_table_t *) phys_to_virt(get_cr3());
    kernel_page_dir = (page_directory_t){.table = kernel_page_table};
    register_interrupt_handler(14, (void *) page_fault_handle, 0, 0x8E);
    kinfo("Kernel page table in 0x%p", kernel_page_table);
}
