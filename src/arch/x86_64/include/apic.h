#pragma once

#define APIC_ICR_LOW  0x300
#define APIC_ICR_HIGH 0x310

#define MADT_APIC_CPU 0x00
#define MADT_APIC_IO  0x01
#define MADT_APIC_INT 0x02
#define MADT_APIC_NMI 0x03

#define LAPIC_REG_ID            0x20
#define LAPIC_REG_TIMER_CURCNT  0x390
#define LAPIC_REG_TIMER_INITCNT 0x380
#define LAPIC_REG_TIMER         0x320
#define LAPIC_REG_SPURIOUS      0xf0
#define LAPIC_REG_TIMER_DIV     0x3e0
#define LAPIC_TIMER_PERIODIC    (1 << 17)
#define LAPIC_TIMER_MASKED      (1 << 16)

#define MAX_IOAPICS 8  // IOAPIC最大支持数(物理机会存在多个IOAPIC)
#define MAX_ISO     64 // 最大支持中断源数

#define LAPIC_TIMER_SPEED 100 // 100Hz

#include "types.h"

struct ioapic_info {
    uint8_t   id;
    uintptr_t mmio_base;
    uint32_t  gsi_base;
    uint32_t  irq_count; /* 通常 IOAPIC 有 24 (或更多) 个 direction entry */
};

struct iso_info {
    uint8_t  bus;
    uint8_t  irq_source;
    uint32_t gsi;
    uint16_t flags;
};

void apic_init();
