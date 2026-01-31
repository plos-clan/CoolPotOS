#pragma once

#include "types.h"

typedef struct Capability {
    uintptr_t base_addr;
} Capability;

Capability capability_new(uintptr_t base_addr);
uint8_t  capability_length(Capability cap);
uint16_t capability_version(Capability cap);
uint32_t capability_db_off(Capability cap);
uint32_t capability_rts_off(Capability cap);
uint8_t  capability_max_slots(Capability cap);
uint8_t  capability_max_ports(Capability cap);
bool     capability_address_64bit(Capability cap);
bool     capability_context_64byte(Capability cap);
uint32_t capability_max_scratchpad_bufs(Capability cap);
