#ifndef CRASHPOWEROS_DESCRIPTION_TABLE_H
#define CRASHPOWEROS_DESCRIPTION_TABLE_H

#include <stdint.h>

#define GDT_LENGTH 6

typedef struct gdt_entry_t {
    uint16_t limit_low;           // 段基址 | 低16位置
    uint16_t base_low;            // 段基址 | 高16位置
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

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

typedef struct tss_table {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss_entry;

void write_tss(int32_t num, uint16_t ss0, uint32_t esp0);

void gdt_install();
void idt_install();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif //CRASHPOWEROS_DESCRIPTION_TABLE_H
