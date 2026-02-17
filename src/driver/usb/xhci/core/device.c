#include "driver/usb/defs/defs.h"
#include "driver/usb/usb_mem.h"
#include "driver/usb/xhci/core/xhci.h"
#include "krlibc.h"
#include "term/klog.h"

static uint32_t ilog2_u32(uint32_t val) {
    if (val == 0) {
        return 0;
    }
    return (uint32_t)(fls(val) - 1);
}

bool xhci_address_device(Xhci *xhci, int port_id, uint8_t slot_id, uint32_t speed_id) {
    kdebug("Addressing device on slot %d...", slot_id);

    uint64_t out_ctx_phys = 0;
    void *out_ctx_virt = usb_alloc_dma_pages(1, &out_ctx_phys);
    xhci->dcbaa_virt[slot_id] = out_ctx_phys;

    TransferRing ep0_ring = transfer_ring_new();
    xhci->slots[slot_id].id = slot_id;
    xhci->slots[slot_id].active = true;
    xhci->slots[slot_id].port_id = port_id;
    xhci->slots[slot_id].speed = speed_id;
    xhci->slots[slot_id].out_ctx_virt = (uint64_t *)out_ctx_virt;
    xhci->slots[slot_id].out_ctx_phys = out_ctx_phys;
    xhci->slots[slot_id].rings[1] = ep0_ring;

    uint64_t in_ctx_phys = 0;
    void *in_ctx_virt = usb_alloc_dma_pages(1, &in_ctx_phys);
    if (!in_ctx_virt) {
        return false;
    }

    InputControlContext *ctrl_ctx = input_control_context_from((uintptr_t)in_ctx_virt);
    ctrl_ctx->add_flags = (1u << 0) | (1u << 1);

    SlotContext *slot_ctx = slot_context_from((uintptr_t)in_ctx_virt, xhci->ctx_size);
    slot_context_set_entries(slot_ctx, 1);
    slot_context_set_root_hub_port(slot_ctx, (uint32_t)port_id);
    slot_context_set_route_string(slot_ctx, 0);
    slot_context_set_speed(slot_ctx, speed_id);

    uint32_t mps = 64;
    if (speed_id == USB_SPEED_SUPER) {
        mps = 512;
    } else if (speed_id == USB_SPEED_LOW) {
        mps = 8;
    }

    EndpointContext *ep0_ctx = endpoint_context_from((uintptr_t)in_ctx_virt, 1, xhci->ctx_size);
    endpoint_context_set_ep_type(ep0_ctx, USB_EP_TYPE_CONTROL + 4);
    endpoint_context_set_max_packet_size(ep0_ctx, mps);
    endpoint_context_set_max_burst(ep0_ctx, 0);
    endpoint_context_set_error_count(ep0_ctx, 3);
    endpoint_context_set_average_trb_len(ep0_ctx, 8);
    endpoint_context_set_dequeue_ptr(ep0_ctx, ep0_ring.phys_addr | 1u);

    Trb cmd = trb_new_address_device(in_ctx_phys, slot_id);
    uint32_t code = 0;
    if (!xhci_send_command(xhci, cmd, &code, NULL)) {
        kerror("Address Device command timeout");
        usb_free_dma_pages(in_ctx_virt, 1);
        return false;
    }

    if (code != 1) {
        kerror("Address Device failed code: %d", code);
        usb_free_dma_pages(in_ctx_virt, 1);
        return false;
    }

    usb_free_dma_pages(in_ctx_virt, 1);
    return true;
}

static uint32_t
xhci_setup_one_endpoint(Xhci *xhci, uint8_t slot_id, uint64_t ctx_base, UsbEndpoint *ep) {
    uint8_t addr = ep->desc.endpoint_address;
    uint8_t ep_num = addr & 0x0f;
    bool is_in = (addr & USB_REQ_DIR_IN) != 0;

    uint32_t dci = is_in ? (ep_num * 2u + 1u) : (ep_num * 2u);
    if (dci < 2 || dci > 31) {
        return 0;
    }

    TransferRing ring = transfer_ring_new();
    xhci->slots[slot_id].rings[dci] = ring;

    uint8_t attr = ep->desc.attributes & 0x3;
    uint32_t ep_type = is_in ? (uint32_t)attr + 4u : (uint32_t)attr;

    uint32_t raw_mps = (uint32_t)ep->desc.max_packet_size;
    uint32_t mps = raw_mps & 0x7ffu;
    uint32_t speed = xhci->slots[slot_id].speed;

    uint32_t error_count = (attr == USB_EP_TYPE_ISO) ? 0u : 3u;

    uint32_t avg_trb_len = 3072;
    if (attr == USB_EP_TYPE_CONTROL) {
        avg_trb_len = 8;
    } else if (attr == USB_EP_TYPE_INT) {
        avg_trb_len = 1024;
    } else if (attr == USB_EP_TYPE_ISO) {
        avg_trb_len = mps;
    }

    bool is_iso_int = (attr == USB_EP_TYPE_INT) || (attr == USB_EP_TYPE_ISO);
    uint32_t hs_burst = is_iso_int ? ((raw_mps >> 11) & 0x03u) : 0u;
    uint32_t ss_burst = ep->has_ss_desc ? (uint32_t)ep->ss_desc.max_burst : 0u;

    uint32_t max_burst = 0;
    if (speed == USB_SPEED_HIGH) {
        max_burst = hs_burst;
    } else if (speed == USB_SPEED_SUPER) {
        max_burst = ss_burst;
    }

    uint32_t raw_ival = ep->desc.interval;
    uint32_t ls_fs_interval = raw_ival > 0 ? ilog2_u32(raw_ival) + 3u : 0u;
    uint32_t hs_ss_interval = raw_ival > 0 ? raw_ival - 1u : 0u;

    uint32_t interval = 0;
    if (is_iso_int) {
        if (speed == USB_SPEED_LOW || speed == USB_SPEED_FULL) {
            interval = ls_fs_interval;
        } else {
            interval = hs_ss_interval;
        }
    }

    bool is_ss_iso = (speed == USB_SPEED_SUPER) && (attr == USB_EP_TYPE_ISO);
    uint32_t ss_iso_mult = ep->has_ss_desc ? (uint32_t)(ep->ss_desc.attributes & 0x3u) : 0u;
    uint32_t mult = is_ss_iso ? ss_iso_mult : 0u;

    uint32_t max_esit_payload = 0;
    if (is_iso_int) {
        if (speed == USB_SPEED_SUPER) {
            if (ep->has_ss_desc) {
                max_esit_payload = ep->ss_desc.bytes_per_interval;
            } else {
                max_esit_payload = mps * (max_burst + 1u);
            }
        } else {
            max_esit_payload = mps * (max_burst + 1u);
        }
    }

    EndpointContext *ep_ctx = endpoint_context_from((uintptr_t)ctx_base, (int)dci, xhci->ctx_size);
    endpoint_context_set_ep_type(ep_ctx, ep_type);
    endpoint_context_set_interval(ep_ctx, interval);
    endpoint_context_set_mult(ep_ctx, mult);
    endpoint_context_set_max_burst(ep_ctx, max_burst);
    endpoint_context_set_error_count(ep_ctx, error_count);
    endpoint_context_set_dequeue_ptr(ep_ctx, ring.phys_addr | 1u);
    endpoint_context_set_max_packet_size(ep_ctx, mps);
    endpoint_context_set_average_trb_len(ep_ctx, avg_trb_len);
    endpoint_context_set_max_esit_payload(ep_ctx, max_esit_payload);

    InputControlContext *ctrl_ctx = input_control_context_from((uintptr_t)ctx_base);
    ctrl_ctx->add_flags |= (1u << dci);

    return dci;
}

bool xhci_configure_endpoints(Xhci *xhci, uint8_t slot_id, UsbEndpointVec *endpoints) {
    uint64_t in_ctx_phys = 0;
    void *in_ctx_virt = usb_alloc_dma_pages(1, &in_ctx_phys);
    if (!in_ctx_virt) {
        return false;
    }

    InputControlContext *ctrl_ctx = input_control_context_from((uintptr_t)in_ctx_virt);
    ctrl_ctx->add_flags = 1u;

    uint32_t max_dci = 0;
    for (size_t i = 0; i < endpoints->len; i++) {
        uint32_t dci = xhci_setup_one_endpoint(
            xhci, slot_id, (uint64_t)(uintptr_t)in_ctx_virt, &endpoints->data[i]);
        if (dci > max_dci) {
            max_dci = dci;
        }
    }

    SlotContext *slot_ctx = slot_context_from((uintptr_t)in_ctx_virt, xhci->ctx_size);
    slot_context_set_entries(slot_ctx, max_dci);

    Trb cmd = trb_new_configure_endpoint(in_ctx_phys, slot_id);
    uint32_t code = 0;
    if (!xhci_send_command(xhci, cmd, &code, NULL)) {
        kerror("Configure endpoint command timeout");
        usb_free_dma_pages(in_ctx_virt, 1);
        return false;
    }

    if (code != 1) {
        kerror("Configure endpoint failed: %d", code);
        usb_free_dma_pages(in_ctx_virt, 1);
        return false;
    }

    usb_free_dma_pages(in_ctx_virt, 1);
    return true;
}

void xhci_cleanup_slot_on_failure(Xhci *xhci, uint8_t slot_id) {
    kdebug("Cleaning up resources for slot %d", slot_id);

    if (xhci->slots[slot_id].active) {
        xhci_disable_slot(xhci, slot_id);
    }
    xhci->slots[slot_id].active = false;

    if (xhci->slots[slot_id].out_ctx_virt) {
        usb_free_dma_pages(xhci->slots[slot_id].out_ctx_virt, 1);
        xhci->slots[slot_id].out_ctx_virt = NULL;
    }

    if (xhci->dcbaa_virt) {
        xhci->dcbaa_virt[slot_id] = 0;
    }

    for (int i = 1; i < 32; i++) {
        if (xhci->slots[slot_id].rings[i].phys_addr != 0) {
            transfer_ring_free(&xhci->slots[slot_id].rings[i]);
        }
    }

    kdebug("Slot %d cleanup complete", slot_id);
}
