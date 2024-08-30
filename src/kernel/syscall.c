#include "../include/syscall.h"
#include "../include/printf.h"
#include "../include/isr.h"
#include "../include/description_table.h"
#include "../include/graphics.h"
#include "../include/io.h"

static void syscall_puchar(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    printf("%c",ebx);
}

static void syscall_print(uint32_t ebx,uint32_t ecx,uint32_t edx,uint32_t esi,uint32_t edi){
    printf("%s",ebx);
}

void *sycall_handlers[MAX_SYSCALLS] = {
        [SYSCALL_PUTC] = syscall_puchar,
        [SYSCALL_PRINT] = syscall_print,
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
    //printf("eax: %08x, ebx: %08x, ecx: %08x,\n edx: %08x, esi: %08x, edi: %08x\n", eax, ebx, ecx, edx, esi, edi);
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