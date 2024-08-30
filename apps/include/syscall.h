#ifndef CRASHPOWEROS_SYSCALL_H
#define CRASHPOWEROS_SYSCALL_H

#define SYSCALL_PUTCHAR 1
#define SYSCALL_PRINT 2

#include <stdint.h>

static inline void syscall_print(char* c){
    uint32_t rets;
    uint32_t __arg1 = c;
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PRINT), "r"(ebx) : "memory", "cc");
}

static inline void syscall_putchar(char c){
    uint32_t rets;
    uint32_t __arg1 = (uint32_t)(c);
    register uint32_t ebx asm("ebx")  = __arg1;
    asm volatile("int $31\n\t" : "=a"(rets) : "0"(SYSCALL_PUTCHAR), "r"(ebx) : "memory", "cc");
}

#endif
