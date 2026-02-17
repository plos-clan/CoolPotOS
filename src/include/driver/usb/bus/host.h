#pragma once

#include "driver/usb/bus/iface.h"
#include "driver/usb/defs/types.h"
#include "types.h"

typedef struct ControlTransferArgs {
    uint8_t slot_id;
    SetupPacket setup;
    uint64_t buffer_phys;
} ControlTransferArgs;

typedef struct GeneralTransferArgs {
    uint8_t slot_id;
    uint8_t ep_addr;
    uint64_t buffer_phys;
    uint32_t length;
} GeneralTransferArgs;

typedef struct HostControllerOps {
    bool (*configure_endpoints)(void *ctx, uint8_t slot_id, UsbEndpointVec *endpoints);
    bool (*submit_control)(void *ctx, ControlTransferArgs args);
    bool (*submit_transfer)(void *ctx, GeneralTransferArgs args);
} HostControllerOps;

typedef struct HostController {
    void *ctx;
    const HostControllerOps *ops;
} HostController;

static inline bool
host_configure_endpoints(HostController *host, uint8_t slot_id, UsbEndpointVec *endpoints) {
    if (!host || !host->ops || !host->ops->configure_endpoints) {
        return false;
    }
    return host->ops->configure_endpoints(host->ctx, slot_id, endpoints);
}

static inline bool host_submit_control(HostController *host, ControlTransferArgs args) {
    if (!host || !host->ops || !host->ops->submit_control) {
        return false;
    }
    return host->ops->submit_control(host->ctx, args);
}

static inline bool host_submit_transfer(HostController *host, GeneralTransferArgs args) {
    if (!host || !host->ops || !host->ops->submit_transfer) {
        return false;
    }
    return host->ops->submit_transfer(host->ctx, args);
}
