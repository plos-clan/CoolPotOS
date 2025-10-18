#include "entry.h"

.section .text

Restore_all:
    // === 恢复调用现场 ===
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp

    popq %rax
    movq %rax, %ds

    popq %rax
    movq %rax, %es

    popq %rax
    addq $0x10, %rsp // 弹出变量FUNC和errcode

    iretq

ENTRY(ret_from_intr)
ENTRY(ret_from_exception)
    cli
    jmp Restore_all