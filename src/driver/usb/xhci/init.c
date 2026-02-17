#include "driver/usb/xhci/init.h"
#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/xhci/regs/cap.h"
#include "driver/usb/xhci/regs/op.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"

Xhci *xhci_temp = NULL;

static void xhci_print_info(Xhci *xhci) {
    uint16_t version  = capability_version(xhci->cap);
    uint8_t max_slots = capability_max_slots(xhci->cap);
    uint8_t max_ports = capability_max_ports(xhci->cap);

    kdebug("xHCI Version: %x.%x", version >> 8, version & 0xff);
    kdebug("Max Slots: %d, Max Ports: %d", max_slots, max_ports);

    if (capability_address_64bit(xhci->cap)) {
        kdebug("Controller supports 64-bit address");
    }
}

static void xhci_init_controller(uintptr_t base_addr) {
    Xhci *xhci = xhci_new(base_addr);
    if (!xhci) {
        return;
    }

    xhci_print_info(xhci);

    if (!xhci_reset_controller(xhci)) {
        kerror("xHCI initialization failed");
        return;
    }

    uint8_t max_slots = capability_max_slots(xhci->cap);
    operational_set_max_slots_enabled(xhci->op, max_slots);

    xhci_setup_dcbaa(xhci, max_slots);
    xhci_setup_command_ring(xhci);
    xhci_setup_interrupter(xhci);

    operational_start(xhci->op);
    kinfo("xHCI Initialized successfully");

    if (!xhci_test_command_ring(xhci)) {
        return;
    }

    xhci_check_ports(xhci);
    xhci_temp = xhci;
}

void xhci_init(pci_device_t *device) {
    pci_bar_t bar = device->bars[0];

    uintptr_t virt = (uintptr_t)phys_to_virt(bar.address);
    page_map_range(get_kernel_pagedir(), virt, bar.address, bar.size, KERNEL_PTE_FLAGS);

    xhci_init_controller(virt);
}
