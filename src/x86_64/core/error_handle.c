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
    printe("ss: 0x%p ", frame->ss);
    printe("cs: 0x%p ", frame->cs);
    printe("rsp: 0x%p \n", frame->rsp);
    printe("rip: 0x%p ", frame->rip);
    printe("rflags: 0x%p ", frame->rflags);
    printe("cr3: 0x%p \n", get_cr3());
}

void print_task_info(pcb_t pcb) {
    if (pcb == NULL)
        printe("Current process PID: 0 (Kernel) CPU%d\n", get_current_cpuid());
    else
        printe("Current process PID: %d (%s) CPU%d\n", pcb->pid, pcb->name, get_current_cpuid());
}

void kernel_error(const char *msg, uint64_t code, interrupt_frame_t *frame) {
    close_interrupt;
    ticket_lock(&error_lock);
    logkf("Kernel Error: %s:0x%x\n", msg, code);
    printe("\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(%s:0x%x)<\033[0m\n", msg,
           code);
    print_task_info(get_current_task());
    print_register(frame);
    update_terminal();
    ticket_unlock(&error_lock);
    for (;;)
        cpu_hlt;
}

__IRQHANDLER void double_fault(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("Double fault", error_code, frame);
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
