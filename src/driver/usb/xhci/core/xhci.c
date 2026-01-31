#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/xhci/regs/port.h"
#include "krlibc.h"
#include "mem/alloc/alloc.h"
#include "term/klog.h"

static bool xhci_host_configure_endpoints(void *ctx, uint8_t slot_id,
                                          UsbEndpointVec *endpoints) {
    return xhci_configure_endpoints((Xhci *)ctx, slot_id, endpoints);
}

const HostControllerOps xhci_host_ops = {
    .configure_endpoints = xhci_host_configure_endpoints,
    .submit_control      = xhci_submit_control,
    .submit_transfer     = xhci_submit_transfer,
};

Xhci *xhci_new(uintptr_t base_addr) {
    Xhci *xhci = (Xhci *)malloc(sizeof(Xhci));
    if (!xhci) {
        return NULL;
    }
    memset(xhci, 0, sizeof(Xhci));

    xhci->cap = capability_new(base_addr);
    uintptr_t op_base = base_addr + capability_length(xhci->cap);
    uintptr_t db_base = base_addr + capability_db_off(xhci->cap);

    xhci->op       = operational_new(op_base);
    xhci->doorbell = doorbell_new(db_base);
    xhci->ctx_size = capability_context_64byte(xhci->cap) ? 64 : 32;

    return xhci;
}

void xhci_poll(Xhci *xhci) {
    bool need_update = false;
    for (int i = 0; i < 16; i++) {
        Trb evt;
        if (!event_ring_pop(&xhci->event_ring, &evt)) {
            break;
        }
        xhci_handle_one_event(xhci, evt);
        need_update = true;
    }
    if (need_update) {
        event_ring_update_erdp(&xhci->event_ring);
    }
}

bool xhci_test_command_ring(Xhci *xhci) {
    kinfo("Testing command ring with no op");

    Trb      cmd  = trb_new_no_op_cmd();
    uint32_t code = 0;
    if (!xhci_send_command(xhci, cmd, &code, NULL)) {
        kerror("No op command timeout or error");
        return false;
    }

    if (code == 1) {
        ksuccess("xHCI command ring verified");
        return true;
    }

    kerror("No op failed with code: %d", code);
    return false;
}

bool xhci_enable_slot(Xhci *xhci, uint8_t *slot_id) {
    Trb      cmd  = trb_new_enable_slot();
    uint32_t code = 0;
    uint8_t  sid  = 0;
    if (!xhci_send_command(xhci, cmd, &code, &sid)) {
        return false;
    }

    if (code != 1) {
        kerror("Failed to enable slot: %d", code);
        return false;
    }

    if (slot_id) {
        *slot_id = sid;
    }
    return true;
}

void xhci_disable_slot(Xhci *xhci, uint8_t slot_id) {
    Trb cmd = trb_new_disable_slot(slot_id);

    uint32_t code = 0;
    if (!xhci_send_command(xhci, cmd, &code, NULL)) {
        kerror("Failed to disable slot: %d", slot_id);
        return;
    }
}

bool xhci_send_command(Xhci *xhci, Trb trb, uint32_t *completion_code, uint8_t *slot_id) {
    command_ring_enqueue(&xhci->cmd_ring, trb);
    doorbell_ring(xhci->doorbell, 0, 0);

    Trb evt;
    if (!xhci_wait_event(xhci, TRB_CMD_COMPLETION, NULL, &evt)) {
        return false;
    }

    if (completion_code) {
        *completion_code = trb_completion_code(evt);
    }
    if (slot_id) {
        *slot_id = trb_slot_id(evt);
    }
    return true;
}

bool xhci_wait_event(Xhci *xhci, uint32_t type, const uint8_t *slot_filter, Trb *out_evt) {
    for (uint32_t i = 0; i < 1000000; i++) {
        Trb evt;
        if (!event_ring_pop(&xhci->event_ring, &evt)) {
            arch_pause();
            continue;
        }
        event_ring_update_erdp(&xhci->event_ring);

        bool type_match = trb_get_type(evt) == type;
        bool slot_match = true;
        if (slot_filter) {
            slot_match = (trb_slot_id(evt) == *slot_filter);
        }

        if (type_match && slot_match) {
            if (out_evt) {
                *out_evt = evt;
            }
            return true;
        }

        xhci_handle_one_event(xhci, evt);
    }
    return false;
}

void xhci_handle_one_event(Xhci *xhci, Trb evt) {
    switch (trb_get_type(evt)) {
    case TRB_TRANSFER_EVENT: {
        uint8_t  slot_id = trb_slot_id(evt);
        uint32_t code    = trb_completion_code(evt);
        uint32_t dci     = trb_endpoint_id(evt);
        uint32_t len     = trb_transfer_length(evt);
        xhci_complete_transfer(xhci, slot_id, dci, code, len);
        break;
    }
    case TRB_PORT_STATUS_CHANGE: {
        uint32_t port_id = evt.param_low >> 24;
        kinfo("Port %d status change", port_id);
        Port port = port_new(xhci->op.base_addr, (int)port_id - 1);
        xhci_handle_port(xhci, port);
        break;
    }
    case TRB_CMD_COMPLETION:
        kdebug("Stale command completion ignored");
        break;
    default:
        kdebug("Ignored event type %d", trb_get_type(evt));
        break;
    }
}
