#include "../include/fpu.h"
#include "../include/memory.h"
#include "../include/task.h"
#include "../include/printf.h"
#include "../include/io.h"

static void fpu_isr_7(registers_t *reg){
    if(!get_current()->fpu_flag) {
        asm volatile("fnclex \n"
                     "fninit \n" ::
                : "memory");
        memset(&(get_current()->context.fpu_regs), 0, sizeof(fpu_regs_t));
    } else {
        asm volatile("frstor (%%eax) \n" ::"a"(&(get_current()->context.fpu_regs)) : "memory");
    }
    get_current()->fpu_flag = 1;
}

void fpu_setup(){
    register_interrupt_handler(7,fpu_isr_7);
    asm volatile("fninit");
    set_cr0(get_cr0() | (1 << 2) | (1 << 3) | (1 << 5));
}