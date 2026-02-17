#pragma once

#include "types.h"

#define XHCI_PORT_CCS (1u << 0)
#define XHCI_PORT_PED (1u << 1)
#define XHCI_PORT_PR (1u << 4)
#define XHCI_PORT_PLS (0xfu << 5)
#define XHCI_PORT_PP (1u << 9)
#define XHCI_PORT_CSC (1u << 17)
#define XHCI_PORT_PRC (1u << 21)
#define XHCI_PORT_RW1C_MASK 0xfe0000u
#define XHCI_PORT_SPEED_SHIFT 10
#define XHCI_PORT_SPEED_MASK 0xfu

typedef struct Port {
    int       id;
    uintptr_t base_addr;
} Port;

Port     port_new(uintptr_t op_base, int index);
bool     port_is_connected(Port port);
bool     port_is_enabled(Port port);
bool     port_has_connect_change(Port port);
bool     port_has_reset_change(Port port);
bool     port_is_in_reset(Port port);
uint32_t port_speed_id(Port port);
bool     port_reset(Port port);
void     port_update_portsc(Port port, uint32_t mask);
