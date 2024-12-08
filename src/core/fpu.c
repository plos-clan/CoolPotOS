#include "fpu.h"
#include "scheduler.h"
#include "io.h"
#include "krlibc.h"
#include "klog.h"

void fpu_handler(registers_t *regs) {
    set_cr0(get_cr0() & ~((1 << 2) | (1 << 3)));
    if (!get_current_proc()->fpu_flag) {
        __asm__ volatile("fnclex \n"
                     "fninit \n"::
                : "memory");
        memset(&(get_current_proc()->context.fpu_regs), 0, sizeof(fpu_regs_t));
    } else {
        __asm__ volatile("frstor (%%eax) \n"::"a"(&(get_current_proc()->context.fpu_regs)) : "memory");
    }
    get_current_proc()->fpu_flag = 1;
}

void switch_fpu(pcb_t *pcb){
    set_cr0(get_cr0() & ~((1 << 2) | (1 << 3)));
    if (pcb->fpu_flag) {
        __asm__ volatile("fnsave (%%eax) \n" ::"a"(&(pcb->context.fpu_regs)) : "memory");
    }
    set_cr0(get_cr0() | (1 << 2) | (1 << 3));
}

void init_fpu(void) {
    register_interrupt_handler(7, fpu_handler);
    __asm__ volatile("fninit");
    set_cr0(get_cr0() | (1 << 2) | (1 << 3) | (1 << 5));
    klogf(true,"FPU coprocessor is initialized.\n");
}