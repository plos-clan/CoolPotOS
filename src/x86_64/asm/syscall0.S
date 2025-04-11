.intel_syntax noprefix
.code64
.section .text

.global asm_syscall_entry
.global asm_syscall_handle

.extern syscall_handle

asm_syscall_handle: //syscall指令式高效系统调用
    SWAPGS
    CALL syscall_handle
    SWAPGS
    SYSRET

asm_syscall_entry: //通用中断陷阱门式系统调用
    PUSH 0 //对齐
    PUSH 0 //对齐
    PUSH R15
    PUSH R14
    PUSH R13
    PUSH R12
    PUSH R11
    PUSH R10
    PUSH R9
    PUSH R8
    PUSH RDI
    PUSH RSI
    PUSH RBP
    PUSH RDX
    PUSH RCX
    PUSH RBX
    PUSH RAX
    MOV  RAX, ES
    PUSH RAX
    MOV  RAX, DS
    PUSH RAX
    MOV  RDI, RSP
    CALL syscall_handle
    MOV RSP, RAX
    POP RAX
    MOV DS, RAX
    POP RAX
    MOV ES, RAX
    POP RAX
    POP RBX
    POP RCX
    POP RDX
    POP RBP
    POP RSI
    POP RDI
    POP R8
    POP R9
    POP R10
    POP R11
    POP R12
    POP R13
    POP R14
    POP R15
    ADD RSP, 16 //越过对齐
    STI
    IRETQ