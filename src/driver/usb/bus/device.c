#include "driver/usb/bus/device.h"
#include "krlibc.h"
#include "mem/alloc/alloc.h"

static uint8_t usb_endpoint_map_index_of(uint8_t ep_addr) {
    uint8_t ep_num = ep_addr & 0x0f;
    bool    is_in  = (ep_addr & USB_REQ_DIR_IN) != 0;
    return (uint8_t)(is_in ? (ep_num + 16) : ep_num);
}

bool usb_endpoint_map_get(const UsbEndpointMap *map, uint8_t ep_addr, uint8_t *iface_idx) {
    uint8_t idx = usb_endpoint_map_index_of(ep_addr);
    if (!map->has_value[idx]) {
        return false;
    }
    if (iface_idx) {
        *iface_idx = map->indices[idx];
    }
    return true;
}

void usb_endpoint_map_set(UsbEndpointMap *map, uint8_t ep_addr, uint8_t iface_idx) {
    uint8_t idx = usb_endpoint_map_index_of(ep_addr);
    map->indices[idx]   = iface_idx;
    map->has_value[idx] = true;
}

UsbDevice *usb_device_new(UsbDeviceConfig cfg) {
    UsbDevice *dev = (UsbDevice *)malloc(sizeof(UsbDevice));
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(UsbDevice));

    dev->host   = cfg.host;
    dev->slot_id = cfg.slot_id;
    dev->port_id = cfg.port_id;
    dev->speed   = cfg.speed;
    UsbInterfaceVec_init(&dev->interfaces);

    return dev;
}

void usb_device_free(UsbDevice *dev) {
    if (!dev) {
        return;
    }
    UsbInterfaceVec_free(&dev->interfaces);
    free(dev);
}
