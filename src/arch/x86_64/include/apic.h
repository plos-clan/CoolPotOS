#pragma once

#include "cptype.h"

struct ioapic_info {
    uint8_t   id;
    uintptr_t mmio_base;
    uint32_t  gsi_base;
    uint32_t  irq_count; /* 通常 IOAPIC 有 24 (或更多) 个红irection entry */
};

struct iso_info {
    uint8_t  bus;
    uint8_t  irq_source;
    uint32_t gsi;
    uint16_t flags;
};

/**
 * 将 IRQ(GSI) 重定向至指定的中断向量
 * @param vector 中断向量
 * @param irq IRQ(GSI)
 */
void ioapic_redirect_irq(uint8_t vector, uint32_t irq);

/**
 * 向 entry 添加指定 IRQ 的电气特性标志
 * @param entry
 * @param isa_irq IRQ
 * @return 添加后的 entry
 */
uint64_t apply_iso_flags(uint64_t entry, uint8_t isa_irq);

/**
 * 根据指定的IRQ从中断源重定向表中获取 GSI
 * @param isa_irq IRQ
 * @return GSI
 */
uint32_t isa_irq_to_gsi(uint8_t isa_irq);
