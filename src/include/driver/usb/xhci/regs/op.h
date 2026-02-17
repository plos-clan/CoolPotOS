#pragma once

#include "types.h"

typedef struct Operational {
    uintptr_t base_addr;
} Operational;

Operational operational_new(uintptr_t base_addr);
uint32_t operational_read_usbcmd(Operational op);
uint32_t operational_read_usbsts(Operational op);
void operational_write_usbsts(Operational op, uint32_t val);
void operational_start(Operational op);
void operational_stop(Operational op);
void operational_reset(Operational op);
bool operational_is_running(Operational op);
bool operational_is_halted(Operational op);
bool operational_not_ready(Operational op);
void operational_set_max_slots_enabled(Operational op, uint8_t num);
void operational_set_dcbaap(Operational op, uint64_t phys_addr);
void operational_set_crcr(Operational op, uint64_t phys_addr);
