#include "kprint.h"
#include "krlibc.h"
#include "isr.h"
#include "io.h"

void print_register(interrupt_frame_t *frame){
    printk("ss: 0x%p ", frame->ss);
    printk("cs: 0x%p ", frame->cs);
    printk("rsp: 0x%p \n", frame->rsp);
    printk("rip: 0x%p ", frame->rip);
    printk("rflags: 0x%p ", frame->rflags);
    printk("cr3: 0x%p \n", get_cr3());
}

void kernel_error(const char *msg,uint64_t code,interrupt_frame_t *frame) {
    close_interrupt;
    printk("\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(%s:0x%x)<\033[0m\n",msg,code);
    print_register(frame);
    for(;;) cpu_hlt;
}

__IRQHANDLER void double_fault(interrupt_frame_t *frame,uint64_t error_code) {
    close_interrupt;
    kernel_error("Double fault",error_code,frame);
}

__IRQHANDLER void divede_error(interrupt_frame_t *frame) {
    close_interrupt;
    kernel_error("Divide by zero error",0,frame);
}

__IRQHANDLER void segment_not_present(interrupt_frame_t *frame,uint64_t error_code) {
    close_interrupt;
    kernel_error("Segment not present",error_code,frame);
}

__IRQHANDLER void invalid_opcode(interrupt_frame_t *frame) {
    close_interrupt;
    kernel_error("Invalid opcode",0,frame);
}

__IRQHANDLER void general_protection_fault(interrupt_frame_t *frame,uint64_t error_code) {
    close_interrupt;
    kernel_error("General protection fault",error_code,frame);
}

void error_setup() {
    register_interrupt_handler(8, (void *) double_fault, 1, 0x8E);
    register_interrupt_handler(0, (void *) divede_error, 0, 0x8E);
    register_interrupt_handler(11, (void *) segment_not_present, 0, 0x8E);
    register_interrupt_handler(6, (void *) invalid_opcode, 0, 0x8E);
    register_interrupt_handler(13, (void *) general_protection_fault, 0, 0x8E);
}
