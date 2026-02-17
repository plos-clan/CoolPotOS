#include "driver/usb/xhci/regs/db.h"

static inline void mmio_out32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

Doorbell doorbell_new(uintptr_t base_addr) {
    Doorbell db = { .base_addr = base_addr };
    return db;
}

void doorbell_ring(Doorbell db, uint8_t slot_id, uint32_t dci) {
    uintptr_t addr = db.base_addr + (uintptr_t)slot_id * 4;
    mmio_out32(addr, dci);
}
