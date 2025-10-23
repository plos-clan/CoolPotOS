#include "description_table.h"
#include "krlibc.h"
#include "ptrace.h"
#include "task/task.h"
#include "term/klog.h"
#include "mem/lazy_alloc.h"
#include "errno.h"

__IRQHANDLER void page_fault_(struct interrupt_frame *frame, uint64_t error_code) {
    arch_close_interrupt();
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
    if(likely(get_current_task() != NULL)) {
        if(get_current_task()->process->pid == 0) goto msg;
        errno_t status = lazy_tryalloc(get_current_task()->process, faulting_address);
        if (status == EOK) {
            arch_open_interrupt();
            return;
        }
        logkf("page_fault process(%s:%d) thread %s:%d\n", get_current_task()->process->name,
               get_current_task()->process->pid, get_current_task()->name, get_current_task()->tid);
        pcb_t process = get_current_task()->process;
        if(process->pid != 0) kill_proc(process,-1,true);
        goto wfi;
    }
msg:;
    char *error_msg = !(error_code & 0x1) ? "NotPresent"
                      : error_code & 0x2  ? "WriteError"
                      : error_code & 0x4  ? "UserMode"
                      : error_code & 0x8  ? "ReservedBitsSet"
                      : error_code & 0x10 ? "DecodeAddress"
                                          : "Unknown";
    kerror("Page %s fault %p at %p", error_msg, faulting_address, frame->rip);
    if (get_current_task() != NULL) {
        printk("Current process(%s:%d) thread %s:%d\n", get_current_task()->process->name,
               get_current_task()->process->pid, get_current_task()->name, get_current_task()->tid);
    }
    arch_close_interrupt();
wfi:
    while(true) arch_wait_for_interrupt();
}

__IRQHANDLER void general_protection_fault(struct interrupt_frame *frame, uint64_t error_code) {

    if(get_current_task() != NULL){
        pcb_t process = get_current_task()->process;
        if(process->pid != 0){
            logkf("general_protection_fault: %x at %p\n\r", error_code, frame->rip);
            logkf("current process(%s:%d) thread:%s:%d\n\r",process->name,process->pid,
                  get_current_task()->name,get_current_task()->tid);
            kill_proc(process,-1,true);
            goto err;
        }
    }else arch_close_interrupt();

    kerror("general_protection_fault: %x at %p", error_code, frame->rip);
err:;
    while(true) arch_wait_for_interrupt();
}

void init_err_handle() {
    register_interrupt_handler(14, page_fault_, 0, 0x8E);
    register_interrupt_handler(13, general_protection_fault, 0, 0x8E);
}
