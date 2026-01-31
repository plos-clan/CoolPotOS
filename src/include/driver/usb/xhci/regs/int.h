#pragma once

#include "types.h"

typedef struct Interrupter {
    uintptr_t base_addr;
} Interrupter;

Interrupter interrupter_new(uintptr_t rt_base, int index);
uintptr_t   interrupter_erdp_addr(Interrupter ir);
void        interrupter_set_erstsz(Interrupter ir, uint32_t size);
void        interrupter_set_erstba(Interrupter ir, uint64_t phys_addr);
void        interrupter_set_erdp(Interrupter ir, uint64_t phys_addr);
void        interrupter_enable(Interrupter ir);
