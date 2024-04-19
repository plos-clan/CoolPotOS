#include "../include/syscall.h"
#include "../include/isr.h"

void syscall_handler(registers_t *regs){
    if (regs->eax >= SYSCALL_NUM)
        return;

    void *location = NULL;//syscalls[regs->eax];

    int ret;
    asm volatile (" \
      push %1; \
      push %2; \
      push %3; \
      push %4; \
      push %5; \
      call *%6; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
    " : "=a" (ret) : "r" (regs->edi), "r" (regs->esi), "r" (regs->edx), "r" (regs->ecx), "r" (regs->ebx), "r" (location));
    regs->eax = ret;
}

void syscall_install(){
    register_interrupt_handler(0x80,syscall_handler);
}