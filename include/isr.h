#ifndef CPOS_ISR_H
#define CPOS_ISR_H

#include <stdint.h>

typedef struct registers {
    uint32_t ds; // 我们自己弹入的，当前数据段
    uint32_t edi, esi, ebp, useless, ebx, edx, ecx, eax; // 按pusha顺序
    // 其中esp一项不会被popa还原至对应寄存器中，因为esp寄存器代表的是当前的栈而不是进入中断前的，因此用useless代替
    uint32_t int_no, err_code; // 中断代号是自己push的，err_code有的自带，有的不自带
    uint32_t eip, cs, eflags, esp, ss; // 进入中断时自动压入的
} registers_t;

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

typedef void (*isr_t)(registers_t *);

void register_interrupt_handler(uint8_t n, isr_t handler);;

#endif
