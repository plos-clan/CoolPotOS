.intel_syntax noprefix
.code64

.section .bss

.comm df_stack, 4096, 16 //DoubleFault 处理器独立栈区 (16字节对齐)

.section .text

.extern double_fault
.extern double_fault_page

.global double_fault_asm

double_fault_asm:
    MOV RAX, qword ptr [double_fault_page]
    MOV CR3, RAX
    LEA RSP, df_stack + 4096
    JMP double_fault
