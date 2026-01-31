#include "driver/usb/xhci/regs/cap.h"

static inline uint8_t mmio_in8(uintptr_t addr) {
    return *(volatile uint8_t *)addr;
}

static inline uint16_t mmio_in16(uintptr_t addr) {
    return *(volatile uint16_t *)addr;
}

static inline uint32_t mmio_in32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static uint32_t capability_hcsparams1(Capability cap) {
    return mmio_in32(cap.base_addr + 0x04);
}

static uint32_t capability_hcsparams2(Capability cap) {
    return mmio_in32(cap.base_addr + 0x08);
}

static uint32_t capability_hccparams1(Capability cap) {
    return mmio_in32(cap.base_addr + 0x10);
}

Capability capability_new(uintptr_t base_addr) {
    Capability cap = { .base_addr = base_addr };
    return cap;
}

uint8_t capability_length(Capability cap) {
    return mmio_in8(cap.base_addr);
}

uint16_t capability_version(Capability cap) {
    return mmio_in16(cap.base_addr + 0x02);
}

uint32_t capability_db_off(Capability cap) {
    return mmio_in32(cap.base_addr + 0x14) & ~0x3u;
}

uint32_t capability_rts_off(Capability cap) {
    return mmio_in32(cap.base_addr + 0x18) & ~0x1fu;
}

uint8_t capability_max_slots(Capability cap) {
    return (uint8_t)(capability_hcsparams1(cap) & 0xffu);
}

uint8_t capability_max_ports(Capability cap) {
    return (uint8_t)(capability_hcsparams1(cap) >> 24);
}

bool capability_address_64bit(Capability cap) {
    return (capability_hccparams1(cap) & 1u) != 0;
}

bool capability_context_64byte(Capability cap) {
    return (capability_hccparams1(cap) & (1u << 2)) != 0;
}

uint32_t capability_max_scratchpad_bufs(Capability cap) {
    uint32_t high = (capability_hcsparams2(cap) >> 21) & 0x1fu;
    uint32_t low  = (capability_hcsparams2(cap) >> 27) & 0x1fu;
    return (high << 5) | low;
}
