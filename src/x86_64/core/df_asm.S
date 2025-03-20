.intel_syntax noprefix
.code64

.section .bss

.comm df_stack, 4096, 16 //DoubleFault 处理器独立栈区 (16字节对齐)

.section .text

.extern double_fault
.global double_fault_asm

double_fault_asm:
    LEA RSP, df_stack + 4096
    JMP double_fault

