#pragma once

#include "types.h"
#include "driver/usb/bus/iface.h"
#include "driver/usb/class/hid/parser.h"

typedef struct HidDevice {
    UsbInterface *iface;
    uint8_t ep_addr;
    uint8_t *report_desc_virt;
    uint64_t report_desc_phys;
    uint16_t report_desc_len;
    uint8_t *buf_virt;
    uint64_t buf_phys;
    uint16_t max_report_size;
    HidDescriptor descriptor;
} HidDevice;

bool hid_device_new(HidDevice *dev, UsbInterface *iface, uint8_t ep_addr);
void hid_device_free(HidDevice *dev);
void hid_device_submit_transfer(HidDevice *dev);
void hid_device_set_protocol(HidDevice *dev, uint16_t proto);
void hid_device_set_idle(HidDevice *dev, uint16_t duration);
bool hid_device_fetch_report_descriptor(HidDevice *dev);
