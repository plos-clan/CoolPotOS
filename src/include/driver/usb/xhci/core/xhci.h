#pragma once

#include "types.h"
#include "driver/usb/bus/host.h"
#include "driver/usb/bus/device.h"
#include "driver/usb/xhci/regs/cap.h"
#include "driver/usb/xhci/regs/op.h"
#include "driver/usb/xhci/regs/db.h"
#include "driver/usb/xhci/regs/port.h"
#include "driver/usb/xhci/core/ctx.h"
#include "driver/usb/xhci/core/ring.h"
#include "driver/usb/xhci/core/slot.h"

#define XHCI_MAX_SLOTS 256

typedef struct Xhci {
    Capability  cap;
    Operational op;
    int         ctx_size;
    uint64_t   *dcbaa_virt;
    CommandRing cmd_ring;
    EventRing   event_ring;
    Doorbell    doorbell;
    Slot        slots[XHCI_MAX_SLOTS];
} Xhci;

extern const HostControllerOps xhci_host_ops;

Xhci *xhci_new(uintptr_t base_addr);

void xhci_poll(Xhci *xhci);

bool xhci_reset_controller(Xhci *xhci);

bool xhci_test_command_ring(Xhci *xhci);

bool xhci_enable_slot(Xhci *xhci, uint8_t *slot_id);
void xhci_disable_slot(Xhci *xhci, uint8_t slot_id);

bool xhci_send_command(Xhci *xhci, Trb trb, uint32_t *completion_code, uint8_t *slot_id);

bool xhci_wait_event(Xhci *xhci, uint32_t type, const uint8_t *slot_filter, Trb *out_evt);

void xhci_handle_one_event(Xhci *xhci, Trb evt);

bool xhci_address_device(Xhci *xhci, int port_id, uint8_t slot_id, uint32_t speed_id);

bool xhci_configure_endpoints(Xhci *xhci, uint8_t slot_id, UsbEndpointVec *endpoints);

bool xhci_submit_transfer(void *ctx, GeneralTransferArgs args);
bool xhci_submit_control(void *ctx, ControlTransferArgs args);

void xhci_complete_transfer(Xhci *xhci, uint8_t slot_id, uint32_t dci, uint32_t code, uint32_t len);

void xhci_check_ports(Xhci *xhci);
void xhci_handle_port(Xhci *xhci, struct Port port);

void xhci_setup_command_ring(Xhci *xhci);
void xhci_setup_dcbaa(Xhci *xhci, uint8_t max_slots);
void xhci_setup_interrupter(Xhci *xhci);
void xhci_cleanup_slot_on_failure(Xhci *xhci, uint8_t slot_id);
