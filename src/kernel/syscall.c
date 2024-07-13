#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"

void syscall_handler(registers_t regs){
    if(regs.eax == 0x01){
        putchar((regs.edx));
    }
    return;
}

void syscall_install(){
    idt_use_reg(80, syscall_handler);
}