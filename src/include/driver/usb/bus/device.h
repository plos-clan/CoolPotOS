#pragma once

#include "types.h"
#include "driver/usb/defs/types.h"
#include "driver/usb/bus/host.h"
#include "driver/usb/bus/iface.h"

typedef struct UsbEndpointMap {
    uint8_t indices[32];
    bool has_value[32];
} UsbEndpointMap;

bool usb_endpoint_map_get(const UsbEndpointMap *map, uint8_t ep_addr, uint8_t *iface_idx);
void usb_endpoint_map_set(UsbEndpointMap *map, uint8_t ep_addr, uint8_t iface_idx);

struct UsbDevice;

typedef struct UsbDevice {
    HostController host;
    uint8_t slot_id;
    int port_id;
    uint32_t speed;
    DeviceDescriptor desc;
    UsbInterfaceVec interfaces;
    UsbEndpointMap ep_map;
} UsbDevice;

typedef struct UsbDeviceConfig {
    HostController host;
    uint8_t slot_id;
    int port_id;
    uint32_t speed;
} UsbDeviceConfig;

UsbDevice *usb_device_new(UsbDeviceConfig cfg);
void usb_device_free(UsbDevice *dev);

bool usb_device_enumerate(UsbDevice *dev);

bool usb_device_submit_control(UsbDevice *dev, struct ControlTransferArgs args);
bool usb_device_submit_transfer(UsbDevice *dev, struct GeneralTransferArgs args);
void usb_device_dispatch_completion(UsbDevice *dev, CompletionEvent event);

void usb_device_match_drivers(UsbDevice *dev);
