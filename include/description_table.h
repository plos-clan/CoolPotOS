#ifndef CPOS_DESCRIPTION_TABLE_H
#define CPOS_DESCRIPTION_TABLE_H

#include <stdint.h>

//8692A可编程中断控制器
#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1

#define IDT_DESC_CNT 0x21
#define ADR_GDT 0x00270000
#define LIMIT_GDT 0x0000ffff
#define AR_DATA32_RW 0x4092
#define AR_CODE32_ER 0x409a

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAG_VAR) asm volatile ("pushfl; popl %0":"=g"(EFLAG_VAR)) 

#define DECLARE_ISR(i) extern void isr##i();

DECLARE_ISR(0)
DECLARE_ISR(1)
DECLARE_ISR(2)
DECLARE_ISR(3)
DECLARE_ISR(4)
DECLARE_ISR(5)
DECLARE_ISR(6)
DECLARE_ISR(7)
DECLARE_ISR(8)
DECLARE_ISR(9)
DECLARE_ISR(10)
DECLARE_ISR(11)
DECLARE_ISR(12)
DECLARE_ISR(13)
DECLARE_ISR(14)
DECLARE_ISR(15)
DECLARE_ISR(16)
DECLARE_ISR(17)
DECLARE_ISR(18)
DECLARE_ISR(19)
DECLARE_ISR(20)
DECLARE_ISR(21)
DECLARE_ISR(22)
DECLARE_ISR(23)
DECLARE_ISR(24)
DECLARE_ISR(25)
DECLARE_ISR(26)
DECLARE_ISR(27)
DECLARE_ISR(28)
DECLARE_ISR(29)
DECLARE_ISR(30)
DECLARE_ISR(31)
#undef DECLARE_ISR
#define DECLARE_IRQ(i) extern void irq##i();
DECLARE_IRQ(0)
DECLARE_IRQ(1)
DECLARE_IRQ(2)
DECLARE_IRQ(3)
DECLARE_IRQ(4)
DECLARE_IRQ(5)
DECLARE_IRQ(6)
DECLARE_IRQ(7)
DECLARE_IRQ(8)
DECLARE_IRQ(9)
DECLARE_IRQ(10)
DECLARE_IRQ(11)
DECLARE_IRQ(12)
DECLARE_IRQ(13)
DECLARE_IRQ(14)
DECLARE_IRQ(15)
#undef DECLARE_IRQ

struct idt_entry_struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

// IDTR
struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

#define NGDT 256 

#define AC_AC 0x1       // 可访问 access
#define AC_RW 0x2       // [代码]可读；[数据]可写 readable for code selector & writeable for data selector
#define AC_DC 0x4       // 方向位 direction
#define AC_EX 0x8       // 可执行 executable, code segment
#define AC_RE 0x10      // 保留位 reserve
#define AC_PR 0x80      // 有效位 persent in memory

// 特权位： 01100000b
#define AC_DPL_KERN 0x00 // RING 0 kernel level
#define AC_DPL_SYST 0x20 // RING 1 systask level
#define AC_DPL_USER 0x60 // RING 3 user level

#define GDT_GR  0x8     // 页面粒度 page granularity, limit in 4k blocks
#define GDT_SZ  0x4     // 大小位 size bt, 32 bit protect mode

// gdt selector 选择子
#define SEL_KCODE   0x1 // 内核代码段
#define SEL_KDATA   0x2 // 内核数据段
#define SEL_UCODE   0x3 // 用户代码段
#define SEL_UDATA   0x4 // 用户数据段
#define SEL_SCODE   0x5 // 用户代码段
#define SEL_SDATA   0x6 // 用户数据段
#define SEL_TSS     0x7 // 任务状态段 task state segment http://wiki.osdev.org/TSS

// RPL 请求特权等级 request privilege level
#define RPL_KERN    0x0
#define RPL_SYST    0x1
#define RPL_USER    0x3

// CPL 当前特权等级 current privilege level
#define CPL_KERN    0x0
#define CPL_SYST    0x1
#define CPL_USER    0x3

struct gdt_entry_struct
{
    uint16_t limit_low;           // 段基址 | 低16位置
    uint16_t base_low;            // 段基址 | 高16位置
    uint8_t base_middle;
    uint8_t access;
    unsigned limit_high: 4;
    unsigned flags: 4;
    uint8_t base_high;
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
    uint16_t limit;               // The upper 16 bits of all selector limits.
    uint32_t base;                // The address of the first gdt_entry_t struct.
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

#include "task.h"

void intr_handle(uint8_t vec_nr);
void gdt_set_gate(uint8_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
void install_gdt();
void install_idt();

#endif