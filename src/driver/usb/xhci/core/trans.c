#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/defs/defs.h"
#include "term/klog.h"

bool xhci_submit_transfer(void *ctx, GeneralTransferArgs args) {
    Xhci *xhci = (Xhci *)ctx;

    uint8_t ep_num = args.ep_addr & 0x0f;
    bool    is_in  = (args.ep_addr & USB_REQ_DIR_IN) != 0;

    uint32_t dci = is_in ? (ep_num * 2u + 1u) : (ep_num * 2u);
    if (dci < 2 || dci > 31) {
        return false;
    }

    Slot *slot = &xhci->slots[args.slot_id];
    TransferRing *ring = &slot->rings[dci];

    Trb trb = trb_new_normal(args.buffer_phys, args.length);

    transfer_ring_enqueue(ring, trb);
    doorbell_ring(xhci->doorbell, args.slot_id, dci);

    return true;
}

bool xhci_submit_control(void *ctx, ControlTransferArgs args) {
    Xhci *xhci = (Xhci *)ctx;

    bool is_in = (args.setup.request_type & USB_REQ_DIR_IN) != 0;

    uint32_t *setup_ptr = (uint32_t *)&args.setup;
    uint32_t  param_low  = setup_ptr[0];
    uint32_t  param_high = setup_ptr[1];

    uint32_t trt = 0;
    if (args.setup.length == 0) {
        trt = 0;
    } else if (is_in) {
        trt = 3;
    } else {
        trt = 2;
    }

    Slot *slot = &xhci->slots[args.slot_id];
    Trb   setup_trb = trb_new_setup_stage(param_low, param_high, trt);
    transfer_ring_enqueue(&slot->rings[1], setup_trb);

    if (args.setup.length > 0) {
        Trb data_trb = trb_new_data_stage(args.buffer_phys, args.setup.length, is_in);
        transfer_ring_enqueue(&slot->rings[1], data_trb);
    }

    bool status_dir_in = (args.setup.length == 0) || !is_in;
    Trb  status_trb    = trb_new_status_stage(status_dir_in);
    transfer_ring_enqueue(&slot->rings[1], status_trb);

    doorbell_ring(xhci->doorbell, args.slot_id, 1);

    Trb evt;
    if (!xhci_wait_event(xhci, TRB_TRANSFER_EVENT, &args.slot_id, &evt)) {
        kerror("Control transfer timeout (slot %d)", args.slot_id);
        return false;
    }

    uint32_t code = trb_completion_code(evt);
    if (code != 1 && code != 13) {
        kerror("Control transfer failed. Code: %d", code);
        return false;
    }

    return true;
}

void xhci_complete_transfer(Xhci *xhci, uint8_t slot_id, uint32_t dci, uint32_t code,
                            uint32_t len) {
    Slot *slot = &xhci->slots[slot_id];

    if (!slot->usb_device) {
        return;
    }

    uint8_t ep_num = (uint8_t)(dci / 2u);
    bool    is_in  = (dci % 2u) != 0;

    TransferStatus status = TRANSFER_STATUS_UNKNOWN;
    switch (code) {
    case 1:
        status = TRANSFER_STATUS_COMPLETED;
        break;
    case 13:
        status = TRANSFER_STATUS_SHORT_PACKET;
        break;
    case 4:
        status = TRANSFER_STATUS_BABBLE;
        break;
    case 5:
        status = TRANSFER_STATUS_TRB_ERROR;
        break;
    case 6:
        status = TRANSFER_STATUS_STALL;
        break;
    default:
        status = TRANSFER_STATUS_UNKNOWN;
        break;
    }

    usb_device_dispatch_completion(
        slot->usb_device,
        (CompletionEvent){
            .status          = status,
            .residual_length = len,
            .ep_addr         = is_in ? (uint8_t)(ep_num | USB_REQ_DIR_IN) : ep_num,
        });
}
