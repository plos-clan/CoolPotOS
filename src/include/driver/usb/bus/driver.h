#pragma once

#include "driver/usb/usb_vec.h"
#include "types.h"

struct UsbInterface;

typedef enum {
    TRANSFER_STATUS_COMPLETED = 0,
    TRANSFER_STATUS_SHORT_PACKET,
    TRANSFER_STATUS_STALL,
    TRANSFER_STATUS_TRB_ERROR,
    TRANSFER_STATUS_BABBLE,
    TRANSFER_STATUS_DATA_ERROR,
    TRANSFER_STATUS_SPLIT_ERROR,
    TRANSFER_STATUS_TIMEOUT,
    TRANSFER_STATUS_DRIVER_ERROR,
    TRANSFER_STATUS_UNKNOWN,
} TransferStatus;

typedef struct {
    uint8_t ep_addr;
    TransferStatus status;
    uint32_t residual_length;
} CompletionEvent;

typedef struct UsbDriver {
    void (*disconnect)(struct UsbDriver *self);
    void (*handle_completion)(struct UsbDriver *self, CompletionEvent event);
} UsbDriver;

typedef UsbDriver *(*ProbeFn)(struct UsbInterface *iface);

USB_VEC_DEFINE(ProbeFn, ProbeFnVec);

extern ProbeFnVec usb_drivers;
