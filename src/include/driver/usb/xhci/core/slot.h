#pragma once

#include "types.h"
#include "driver/usb/xhci/core/ring.h"

struct UsbDevice;

typedef struct Slot {
    uint8_t  id;
    bool     active;
    int      port_id;
    uint32_t speed;
    struct UsbDevice *usb_device;
    uint64_t *out_ctx_virt;
    uint64_t  out_ctx_phys;
    TransferRing rings[32];
} Slot;
