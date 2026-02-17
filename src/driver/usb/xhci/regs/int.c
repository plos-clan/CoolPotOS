#include "driver/usb/xhci/regs/int.h"

static inline uint32_t mmio_in32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void mmio_out32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

#define IR_IMAN_OFF 0x00
#define IR_IMOD_OFF 0x04
#define IR_ERSTSZ_OFF 0x08
#define IR_ERSTBA_OFF 0x10
#define IR_ERDP_OFF 0x18

Interrupter interrupter_new(uintptr_t rt_base, int index) {
    Interrupter ir = {.base_addr = rt_base + 0x20 + (uintptr_t)(index * 32)};
    return ir;
}

uintptr_t interrupter_erdp_addr(Interrupter ir) {
    return ir.base_addr + IR_ERDP_OFF;
}

void interrupter_set_erstsz(Interrupter ir, uint32_t size) {
    mmio_out32(ir.base_addr + IR_ERSTSZ_OFF, size);
}

void interrupter_set_erstba(Interrupter ir, uint64_t phys_addr) {
    uint32_t low = (uint32_t)(phys_addr & 0xffffffffu);
    uint32_t high = (uint32_t)(phys_addr >> 32);
    mmio_out32(ir.base_addr + IR_ERSTBA_OFF, low);
    mmio_out32(ir.base_addr + IR_ERSTBA_OFF + 4, high);
}

void interrupter_set_erdp(Interrupter ir, uint64_t phys_addr) {
    uint32_t low = (uint32_t)(phys_addr & 0xffffffffu);
    uint32_t high = (uint32_t)(phys_addr >> 32);
    mmio_out32(ir.base_addr + IR_ERDP_OFF, low);
    mmio_out32(ir.base_addr + IR_ERDP_OFF + 4, high);
}

void interrupter_enable(Interrupter ir) {
    uint32_t val = mmio_in32(ir.base_addr + IR_IMAN_OFF);
    mmio_out32(ir.base_addr + IR_IMAN_OFF, val | 0x3u);
}
