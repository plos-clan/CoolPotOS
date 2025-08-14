#include "fsgsbase.h"
#include "gop.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pcb.h"
#include "smp.h"
#include "sprintf.h"
#include "terminal.h"

spin_t error_lock = SPIN_INIT;

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
        printk("No process load, CPU%d\n", current_cpu->id);
    else
        printk("Current process PID: %d:%s (%s) CPU%d\n", pcb->tid, pcb->name,
               pcb->parent_group->name, current_cpu->id);
}

void kernel_error(const char *msg, uint64_t code, interrupt_frame_t *frame) {
    close_interrupt;
    disable_scheduler();
    init_print_lock();
    spin_lock(error_lock);
    terminal_close_flush();
    logkf("Kernel Error: %s:0x%x CPU%d %p\n", msg, code, current_cpu->id, frame->rip); // 679a0
    if (get_current_task() == NULL) {
        logkf("Current process PID: NULL CPU%d\n", current_cpu->id);
    } else {
        logkf("Current process PID: %d:%s (%s) CPU%d\n", get_current_task()->tid,
              get_current_task()->name, get_current_task()->parent_group->name, current_cpu->id);
        if (get_current_task()->parent_group->task_level == TASK_APPLICATION_LEVEL)
            kill_proc(get_current_task()->parent_group, -1, true);
        spin_unlock(error_lock);
        terminal_open_flush();
        enable_scheduler();
        cpu_hlt;
    }
    printk("\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(%s:0x%x)<\033[0m\n", msg,
           code);
    print_task_info(get_current_task());
    print_register(frame);
    update_terminal();
    terminal_open_flush();
    spin_unlock(error_lock);
    for (;;)
        cpu_hlt;
}

void not_null_assets(void *ptr, const char *message) {
    if (ptr == NULL) {
        close_interrupt;
        disable_scheduler();
        interrupt_frame_t frame = get_current_registers();
        char              buf[1024];
        sprintf(buf, "KERNEL_NULL_HANDLE: %s\n", message);
        buf[1023] = '\0';
        kernel_error(buf, 0x0, &frame);
    }
}

extern uint64_t double_fault_page;

static uint8_t double_fault_stack[0x1000] __attr(aligned(0x1000));

__IRQHANDLER void double_fault(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    __asm__ volatile("mov %0, %%cr3" : : "r"(double_fault_page) : "memory");
    __asm__ volatile("mov %0, %%rsp" : : "r"(double_fault_stack + 0x1000) : "memory");
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

__IRQHANDLER void float_exception_handler(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    kernel_error("Float Fault", error_code, frame);
}

void error_setup() {
    register_interrupt_handler(0, (void *)dived_error, 0, 0x8E);
    register_interrupt_handler(6, (void *)invalid_opcode, 0, 0x8E);
    register_interrupt_handler(7, (void *)device_not_available, 0, 0x8E);
    register_interrupt_handler(8, (void *)double_fault, 1, 0x8E);
    register_interrupt_handler(10, (void *)invalid_tss, 0, 0x8E);
    register_interrupt_handler(11, (void *)segment_not_present, 0, 0x8E);
    register_interrupt_handler(12, (void *)stack_segment_fault, 0, 0x8E);
    register_interrupt_handler(13, (void *)general_protection_fault, 0, 0x8E);
    register_interrupt_handler(16, float_exception_handler, 0, 0x8E);
}
