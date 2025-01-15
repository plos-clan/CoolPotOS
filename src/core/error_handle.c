#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"
#include "isr.h"

static void print_register(interrupt_frame_t *frame){
    printk("rax: 0x%p ", frame->rax);
    printk("rbx: 0x%p ", frame->rbx);
    printk("rcx: 0x%p ", frame->rcx);
    printk("rdx: 0x%p \n", frame->rdx);
    printk("rsi: 0x%p ", frame->rsi);
    printk("rdi: 0x%p ", frame->rdi);
    printk("rbp: 0x%p ", frame->rbp);
    printk("rsp: 0x%p \n", frame->rsp);
    printk("rip: 0x%p ", frame->rip);
    printk("rflags: 0x%p \n", frame->rflags);
}

static void kernel_error(const char *msg,uint64_t code,interrupt_frame_t *frame) {
    printk("\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(%s:0x%x)<\033[0m\n",msg,code);
    print_register(frame);
    for(;;) cpu_hlt;
}

__IRQHANDLER void double_fault(interrupt_frame_t *frame,uint64_t error_code) {
    kernel_error("Double fault",error_code,frame);
}

__IRQHANDLER void divede_error(interrupt_frame_t *frame) {
    kernel_error("Divide by zero error",0,frame);
}

__IRQHANDLER void segment_not_present(interrupt_frame_t *frame,uint64_t error_code) {
    kernel_error("Segment not present",error_code,frame);
}

__IRQHANDLER void invalid_opcode(interrupt_frame_t *frame) {
    kernel_error("Invalid opcode",0,frame);
}

__IRQHANDLER void general_protection_fault(interrupt_frame_t *frame,uint64_t error_code) {
    kernel_error("General protection fault",error_code,frame);
}

void error_setup() {
    register_interrupt_handler(8, (void *) double_fault, 1, 0x8E);
    register_interrupt_handler(0, (void *) divede_error, 0, 0x8E);
    register_interrupt_handler(11, (void *) segment_not_present, 0, 0x8E);
    register_interrupt_handler(6, (void *) invalid_opcode, 0, 0x8E);
    register_interrupt_handler(13, (void *) general_protection_fault, 0, 0x8E);
}
