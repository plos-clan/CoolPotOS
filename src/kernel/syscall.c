#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"
#include "../include/io.h"

void syscall_handler(registers_t regs){
    io_cli();
    register uint32_t eax asm("eax");
    printf("Syscall is enable. EAX: %08x PHIEAX: %08x\n",regs.eax,eax);
    if(regs.eax == 0x01){
        putchar((regs.edx));
    }
    io_sti();
    return;
}

void *sycall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = putchar,
        [SYSCALL_PRINT] = print,
};

typedef size_t (*syscall_t)(size_t, size_t, size_t, size_t, size_t);

size_t syscall() {
    volatile size_t eax, ebx, ecx, edx, esi, edi;
    asm("mov %%eax, %0\n\t" : "=r"(eax));
    asm("mov %%ebx, %0\n\t" : "=r"(ebx));
    asm("mov %%ecx, %0\n\t" : "=r"(ecx));
    asm("mov %%edx, %0\n\t" : "=r"(edx));
    asm("mov %%esi, %0\n\t" : "=r"(esi));
    asm("mov %%edi, %0\n\t" : "=r"(edi));
    printf("eax: %d, ebx: %d, ecx: %d, edx: %d, esi: %d, edi: %d\n", eax, ebx, ecx, edx, esi, edi);
    if (0 <= eax && eax < MAX_SYSCALLS && sycall_handlers[eax] != NULL) {
        eax = ((syscall_t)sycall_handlers[eax])(ebx, ecx, edx, esi, edi);
    } else {
        eax = -1;
    }
    return eax;
}

void syscall_install(){
    extern void asm_syscall_handler();
    idt_use_reg(31,asm_syscall_handler);
}