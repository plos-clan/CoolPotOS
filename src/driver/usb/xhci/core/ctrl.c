#include "driver/usb/xhci/core/xhci.h"
#include "krlibc.h"
#include "term/klog.h"

static bool xhci_wait_ready(Xhci *xhci) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if (!operational_not_ready(xhci->op)) {
            return true;
        }
        arch_pause();
    }
    return false;
}

static bool xhci_wait_halted(Xhci *xhci) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if (operational_is_halted(xhci->op)) {
            return true;
        }
        arch_pause();
    }
    return false;
}

static bool xhci_wait_reset_complete(Xhci *xhci) {
    for (uint32_t i = 0; i < 1000000; i++) {
        if ((operational_read_usbcmd(xhci->op) & 2u) == 0) {
            return true;
        }
        arch_pause();
    }
    return false;
}

bool xhci_reset_controller(Xhci *xhci) {
    kdebug("Resetting xHCI controller");

    if (operational_is_running(xhci->op)) {
        kdebug("Controller is running, stopping");
        operational_stop(xhci->op);

        if (!xhci_wait_halted(xhci)) {
            kerror("Failed to stop controller");
            return false;
        }
    }

    operational_reset(xhci->op);

    if (!xhci_wait_reset_complete(xhci)) {
        kerror("Reset timeout");
        return false;
    }

    if (!xhci_wait_ready(xhci)) {
        kerror("Controller stuck in not ready state");
        return false;
    }

    kdebug("xHCI controller reset complete");
    return true;
}
