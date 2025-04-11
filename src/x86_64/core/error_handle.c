#include "gop.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pcb.h"
#include "smp.h"
#include "terminal.h"

extern void double_fault_asm(); // df_asm.S

ticketlock error_lock;

void print_register(interrupt_frame_t *frame) {
    printk("ss: 0x%p ", frame->ss);
    printk("cs: 0x%p ", frame->cs);
    printk("rsp: 0x%p \n", frame->rsp);
    printk("rip: 0x%p ", frame->rip);
    printk("rflags: 0x%p ", frame->rflags);
    printk("cr3: 0x%p \n", get_cr3());
}

void print_task_info(tcb_t pcb) {
    if (pcb == NULL)
        printk("No process load, CPU%d\n", cpu->id);
    else
        printk("Current process PID: %d:%s (%s) CPU%d\n", pcb->pid, pcb->name,
               pcb->parent_group->name, cpu->id);
}

void kernel_error(const char *msg, uint64_t code, interrupt_frame_t *frame) {
    close_interrupt;
    disable_scheduler();
    init_print_lock();
    ticket_lock(&error_lock);
    logkf("Kernel Error: %s:0x%x\n", msg, code);
    logkf("Current process PID: %d:%s (%s) CPU%d\n", get_current_task()->pid,
          get_current_task()->name, get_current_task()->parent_group->name, cpu->id);
    printk("\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(%s:0x%x)<\033[0m\n", msg,
           code);
    print_task_info(get_current_task());
    print_register(frame);
    update_terminal();
    ticket_unlock(&error_lock);
    for (;;)
        cpu_hlt;
}

void not_null_assets(void *ptr) {
    if (ptr == NULL) {
        close_interrupt;
        disable_scheduler();
        interrupt_frame_t frame = get_current_registers();
        kernel_error("KERNEL_NULL_HANDLE", 0x0, &frame);
    }
}

__IRQHANDLER void double_fault(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    disable_scheduler();
    logkf("Double Fault\n");
    gop_clear(0x7c0d0d);
    cpu_hlt;
}

__IRQHANDLER void dived_error(interrupt_frame_t *frame) {
    close_interrupt;
    kernel_error("Divide by zero error", 0, frame);
}

__IRQHANDLER void segment_not_present(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("Segment not present", error_code, frame);
}

__IRQHANDLER void invalid_opcode(interrupt_frame_t *frame) {
    close_interrupt;
    kernel_error("Invalid opcode", 0, frame);
}

__IRQHANDLER void general_protection_fault(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("General protection fault", error_code, frame);
}

__IRQHANDLER void device_not_available(interrupt_frame_t *frame) {
    close_interrupt;
    kernel_error("Device Not Available", 0, frame);
}

__IRQHANDLER void invalid_tss(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("Invalid TSS", error_code, frame);
}

__IRQHANDLER void stack_segment_fault(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("Stack Segment Fault", error_code, frame);
}

void error_setup() {
    register_interrupt_handler(0, (void *)dived_error, 0, 0x8E);
    register_interrupt_handler(6, (void *)invalid_opcode, 0, 0x8E);
    register_interrupt_handler(7, (void *)device_not_available, 0, 0x8E);
    register_interrupt_handler(8, (void *)double_fault_asm, 1, 0x8E);
    register_interrupt_handler(10, (void *)invalid_tss, 0, 0x8E);
    register_interrupt_handler(11, (void *)segment_not_present, 0, 0x8E);
    register_interrupt_handler(12, (void *)stack_segment_fault, 0, 0x8E);
    register_interrupt_handler(13, (void *)general_protection_fault, 0, 0x8E);
}
