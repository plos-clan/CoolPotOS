#pragma once

#include "types.h"

typedef struct Doorbell {
    uintptr_t base_addr;
} Doorbell;

Doorbell doorbell_new(uintptr_t base_addr);
void     doorbell_ring(Doorbell db, uint8_t slot_id, uint32_t dci);
