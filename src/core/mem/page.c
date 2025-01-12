#include "page.h"
#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"

static void page_fault_handle() {
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    kerror("Page fault, virtual address 0x%x\n", faulting_address);
    cpu_hlt;
}

void page_setup() {
    register_interrupt_handler(14, (void *) page_fault_handle, 0, 0x8E);
}
