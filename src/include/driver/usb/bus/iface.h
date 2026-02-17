#pragma once

#include "driver/usb/bus/driver.h"
#include "driver/usb/defs/defs.h"
#include "driver/usb/defs/types.h"
#include "driver/usb/usb_vec.h"

struct UsbDevice;

typedef struct UsbEndpoint {
    EndpointDescriptor desc;
    bool has_ss_desc;
    SsEndpointCompanionDescriptor ss_desc;
} UsbEndpoint;

USB_VEC_DEFINE(UsbEndpoint, UsbEndpointVec);

typedef struct UsbExtraData {
    uint16_t hid_report_desc_len;
} UsbExtraData;

typedef struct UsbInterface {
    struct UsbDevice *device;
    InterfaceDescriptor desc;
    UsbDriver *driver;
    UsbEndpointVec endpoints;
    UsbExtraData extra_data;
} UsbInterface;

USB_VEC_DEFINE(UsbInterface, UsbInterfaceVec);

bool usb_interface_matches(const UsbInterface *iface, uint8_t class_id, uint8_t sub, uint8_t proto);
UsbEndpoint *usb_interface_find_endpoint(UsbInterface *iface, uint8_t ep_type, bool is_in);
