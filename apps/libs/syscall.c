#include "../include/syscall.h"

void syscall_print(char* c){
    uint32_t rets;
    uint32_t __arg1 = c;
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PRINT), "r"(ebx) : "memory", "cc");
}

void syscall_putchar(char c){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(c);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PUTC), "r"(ebx) : "memory", "cc");
}

char syscall_getc(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_GETC) : "memory", "cc");
    return rets;
}

void* syscall_malloc(size_t size){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(size);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_MALLOC), "r"(ebx) : "memory", "cc");
    return rets;
}

void syscall_free(void *ptr){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(ptr);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_FREE), "r"(ebx) : "memory", "cc");
}

void syscall_exit(int code){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(code);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_EXIT), "r"(ebx)  : "memory", "cc");\
}

void syscall_g_clean(){
    uint32_t rets;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_G_CLEAN) : "memory", "cc");
}