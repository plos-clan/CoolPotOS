#include "description_table.h"
#include "krlibc.h"
#include "ptrace.h"
#include "term/klog.h"

__IRQHANDLER void page_fault_(struct interrupt_frame *frame, uint64_t error_code){
    kerror("Page fault at %p",frame->rip);
    arch_close_interrupt();
    arch_wait_for_interrupt();
}

void init_err_handle(){
    register_interrupt_handler(9, page_fault_, 0, 0x8E);
}
