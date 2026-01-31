#include "driver/usb/xhci/regs/port.h"

static inline uint32_t mmio_in32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void mmio_out32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

Port port_new(uintptr_t op_base, int index) {
    Port port = {
        .id        = index + 1,
        .base_addr = op_base + 0x400 + (uintptr_t)(index * 16),
    };
    return port;
}

static uint32_t port_read_portsc(Port port) {
    return mmio_in32(port.base_addr);
}

bool port_is_connected(Port port) {
    return (port_read_portsc(port) & XHCI_PORT_CCS) != 0;
}

bool port_is_enabled(Port port) {
    return (port_read_portsc(port) & XHCI_PORT_PED) != 0;
}

bool port_has_connect_change(Port port) {
    return (port_read_portsc(port) & XHCI_PORT_CSC) != 0;
}

bool port_has_reset_change(Port port) {
    return (port_read_portsc(port) & XHCI_PORT_PRC) != 0;
}

bool port_is_in_reset(Port port) {
    return (port_read_portsc(port) & XHCI_PORT_PR) != 0;
}

uint32_t port_speed_id(Port port) {
    return (port_read_portsc(port) >> XHCI_PORT_SPEED_SHIFT) & XHCI_PORT_SPEED_MASK;
}

bool port_reset(Port port) {
    if (!port_is_connected(port)) {
        return false;
    }
    port_update_portsc(port, XHCI_PORT_PR | XHCI_PORT_PP);
    return true;
}

void port_update_portsc(Port port, uint32_t mask) {
    uint32_t val      = port_read_portsc(port);
    uint32_t write_val = (val & ~XHCI_PORT_RW1C_MASK) | mask;
    mmio_out32(port.base_addr, write_val);
}
