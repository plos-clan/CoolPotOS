#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/bus/device.h"
#include "term/klog.h"
#include "krlibc.h"

static bool xhci_setup_slot_device(Xhci *xhci, Port port, uint8_t slot_id);

static bool xhci_wait_port_reset(Port port) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if (port_has_reset_change(port)) {
            port_update_portsc(port, XHCI_PORT_PRC);
            if (port_is_enabled(port)) {
                return true;
            }
        }
        if (!port_is_in_reset(port) && port_is_enabled(port)) {
            return true;
        }
        arch_pause();
    }

    kerror("Port %d reset timeout", port.id);
    return false;
}

void xhci_check_ports(Xhci *xhci) {
    uint8_t max_ports = capability_max_ports(xhci->cap);
    kinfo("Scanning %d USB ports...", max_ports);

    for (uint8_t i = 0; i < max_ports; i++) {
        Port port = port_new(xhci->op.base_addr, i);
        xhci_handle_port(xhci, port);
    }
}

void xhci_handle_port(Xhci *xhci, Port port) {
    if (!port_is_connected(port)) {
        return;
    }

    if (port_has_connect_change(port)) {
        port_update_portsc(port, XHCI_PORT_CSC);
    }

    if (port_is_enabled(port)) {
        kdebug("Port %d already enabled", port.id);
        return;
    }

    kdebug("Port %d connected, resetting...", port.id);
    if (!port_reset(port) || !xhci_wait_port_reset(port)) {
        kwarn("Port %d reset failed", port.id);
        return;
    }

    uint8_t slot_id = 0;
    if (!xhci_enable_slot(xhci, &slot_id)) {
        kerror("Failed to enable slot for port %d", port.id);
        return;
    }

    kinfo("Device assigned to slot %d", slot_id);

    if (!xhci_setup_slot_device(xhci, port, slot_id)) {
        kerror("Device init failed for slot %d", slot_id);
        xhci_cleanup_slot_on_failure(xhci, slot_id);
    }
}

static bool xhci_setup_slot_device(Xhci *xhci, Port port, uint8_t slot_id) {
    uint32_t speed_id = port_speed_id(port);
    kinfo("Port %d enabled (speed: %d)", port.id, speed_id);

    if (!xhci_address_device(xhci, port.id, slot_id, speed_id)) {
        return false;
    }

    UsbDevice *dev = usb_device_new((UsbDeviceConfig){
        .host    = { .ctx = xhci, .ops = &xhci_host_ops },
        .slot_id = slot_id,
        .port_id = port.id,
        .speed   = speed_id,
    });

    xhci->slots[slot_id].usb_device = dev;

    if (!usb_device_enumerate(dev)) {
        kerror("Enumeration failed for slot %d", slot_id);
        usb_device_free(dev);
        xhci->slots[slot_id].usb_device = NULL;
        return false;
    }

    return true;
}
