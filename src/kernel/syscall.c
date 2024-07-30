#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"
#include "../include/io.h"

extern asm_syscall_handler();

void syscall_handler(registers_t regs){
    io_cli();
    printf("Syscall is enable.\n");
    if(regs.eax == 0x01){
        putchar((regs.edx));
    }
    io_sti();
    return;
}

void syscall_install(){
    register_interrupt_handler(31,asm_syscall_handler);
}