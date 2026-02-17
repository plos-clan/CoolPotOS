#include "driver/usb/xhci/regs/op.h"

static inline uint32_t mmio_in32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void mmio_out32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

#define OP_USBCMD_OFF 0x00
#define OP_USBSTS_OFF 0x04
#define OP_DNCTRL_OFF 0x14
#define OP_CRCR_OFF 0x18
#define OP_DCBAAP_OFF 0x30
#define OP_CONFIG_OFF 0x38

Operational operational_new(uintptr_t base_addr) {
    Operational op = { .base_addr = base_addr };
    return op;
}

uint32_t operational_read_usbcmd(Operational op) {
    return mmio_in32(op.base_addr + OP_USBCMD_OFF);
}

uint32_t operational_read_usbsts(Operational op) {
    return mmio_in32(op.base_addr + OP_USBSTS_OFF);
}

static void operational_write_usbcmd(Operational op, uint32_t val) {
    mmio_out32(op.base_addr + OP_USBCMD_OFF, val);
}

void operational_write_usbsts(Operational op, uint32_t val) {
    mmio_out32(op.base_addr + OP_USBSTS_OFF, val);
}

void operational_start(Operational op) {
    uint32_t val = operational_read_usbcmd(op);
    operational_write_usbcmd(op, val | 1u);
}

void operational_stop(Operational op) {
    uint32_t val = operational_read_usbcmd(op);
    operational_write_usbcmd(op, val & ~1u);
}

void operational_reset(Operational op) {
    uint32_t val = operational_read_usbcmd(op);
    operational_write_usbcmd(op, val | 2u);
}

bool operational_is_running(Operational op) {
    return (operational_read_usbcmd(op) & 1u) != 0;
}

bool operational_is_halted(Operational op) {
    return (operational_read_usbsts(op) & 1u) != 0;
}

bool operational_not_ready(Operational op) {
    return (operational_read_usbsts(op) & (1u << 11)) != 0;
}

static uint32_t operational_read_config(Operational op) {
    return mmio_in32(op.base_addr + OP_CONFIG_OFF);
}

static void operational_write_config(Operational op, uint32_t val) {
    mmio_out32(op.base_addr + OP_CONFIG_OFF, val);
}

void operational_set_max_slots_enabled(Operational op, uint8_t num) {
    uint32_t val = operational_read_config(op) & ~0xffu;
    operational_write_config(op, val | (uint32_t)num);
}

void operational_set_dcbaap(Operational op, uint64_t phys_addr) {
    uint32_t low  = (uint32_t)(phys_addr & 0xffffffffu);
    uint32_t high = (uint32_t)(phys_addr >> 32);
    mmio_out32(op.base_addr + OP_DCBAAP_OFF, low);
    mmio_out32(op.base_addr + OP_DCBAAP_OFF + 4, high);
}

void operational_set_crcr(Operational op, uint64_t phys_addr) {
    uint32_t low  = (uint32_t)(phys_addr & 0xffffffffu) | 1u;
    uint32_t high = (uint32_t)(phys_addr >> 32);
    mmio_out32(op.base_addr + OP_CRCR_OFF, low);
    mmio_out32(op.base_addr + OP_CRCR_OFF + 4, high);
}
