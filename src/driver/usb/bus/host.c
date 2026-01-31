#include "driver/usb/bus/device.h"

bool usb_device_submit_control(UsbDevice *dev, ControlTransferArgs args) {
    if (!dev) {
        return false;
    }

    ControlTransferArgs final_args = args;
    final_args.slot_id = dev->slot_id;
    return host_submit_control(&dev->host, final_args);
}

bool usb_device_submit_transfer(UsbDevice *dev, GeneralTransferArgs args) {
    if (!dev) {
        return false;
    }

    GeneralTransferArgs final_args = args;
    final_args.slot_id = dev->slot_id;
    return host_submit_transfer(&dev->host, final_args);
}

void usb_device_dispatch_completion(UsbDevice *dev, CompletionEvent event) {
    if (!dev) {
        return;
    }

    uint8_t iface_idx = 0;
    if (!usb_endpoint_map_get(&dev->ep_map, event.ep_addr, &iface_idx)) {
        return;
    }

    UsbInterface *iface = UsbInterfaceVec_get(&dev->interfaces, iface_idx);
    if (!iface || !iface->driver || !iface->driver->handle_completion) {
        return;
    }

    iface->driver->handle_completion(iface->driver, event);
}
