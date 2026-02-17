#include "driver/usb/init.h"
#include "driver/pci/pci.h"
#include "driver/usb/class/init.h"
#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/xhci/init.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

static bool has_usb_device = false;

static void usb_load_device(pci_device_t *device) {
    uint8_t prog_if = (uint8_t)(device->class_code & 0xff);

    switch (prog_if) {
    case 0x30:
        kinfo("Found xHCI controller");
        has_usb_device = true;
        xhci_init(device);
        break;
    default:
        kwarn("Unknown USB interface: %x", prog_if);
        break;
    }
}

static void xhci_poll_loop() {
    while (true) {
        xhci_poll(xhci_temp);
    }
}

void usb_init(void) {
    kinfo("Initializing USB subsystem...");
    usb_class_init();

    pci_find_class(0x0c0300, usb_load_device);
}

void usb_kservice_setup() {
    if (!has_usb_device)
        return;
    create_kernel_thread("usb_service", (void *)xhci_poll_loop, NULL, NULL, NICE_TO_PRIO(0));
}
