section .multiboot

dd 0x1badb002 ; 文件魔术头, 内核识别码
dd 7          ; FLAGS
dd -464367625 ;-(MAGIC + FLAGS)
dd 0          ;HEADER_ADDR
dd 0          ;LOAD_ADDR
dd 0          ;LOAD_END_ADDR
dd 0          ;BSS_END_ADDR
dd 0          ;ENTRY_ADDR
dd 0          ;MODE_TYPE
dd 1280       ;WIDTH
dd 768        ;HEIGHT
dd 32         ;DEPTH
times 52 db 0 ; 显式初始化 52 个字节为零

section .text
global _start
extern kernel_head ; 内核预处理函数 src/core/mboot/kernel_head.c

_start:
    mov esp,stack_top
    push esp
    push ebx
    call kernel_head
L1:
    hlt
    jmp L1


section .bss
stack_bottom:
resb 16384 ; 内核栈大小 16KB
stack_top: