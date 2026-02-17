#include "driver/usb/bus/iface.h"

bool usb_interface_matches(
    const UsbInterface *iface, uint8_t class_id, uint8_t sub, uint8_t proto
) {
    const InterfaceDescriptor *desc = &iface->desc;
    return (class_id == 0xff || desc->interface_class == class_id)
           && (sub == 0xff || desc->interface_subclass == sub)
           && (proto == 0xff || desc->interface_protocol == proto);
}

UsbEndpoint *usb_interface_find_endpoint(UsbInterface *iface, uint8_t ep_type, bool is_in) {
    uint8_t ep_dir = is_in ? USB_REQ_DIR_IN : 0;

    for (size_t i = 0; i < iface->endpoints.len; i++) {
        UsbEndpoint *ep  = &iface->endpoints.data[i];
        uint8_t cur_dir  = ep->desc.endpoint_address & USB_REQ_DIR_IN;
        uint8_t cur_type = ep->desc.attributes & 0x03;

        if (cur_dir == ep_dir && cur_type == ep_type) {
            return ep;
        }
    }

    return NULL;
}
