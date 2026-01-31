#include "driver/usb/class/hid/common.h"
#include "driver/usb/bus/device.h"
#include "driver/usb/defs/defs.h"
#include "driver/usb/usb_mem.h"
#include "krlibc.h"
#include "term/klog.h"

bool hid_device_fetch_report_descriptor(HidDevice *dev);

bool hid_device_new(HidDevice *dev, UsbInterface *iface, uint8_t ep_addr) {
    uint16_t desc_len = iface->extra_data.hid_report_desc_len;

    if (desc_len == 0) {
        kerror("HID: report descriptor length is 0");
        return false;
    }

    uint64_t desc_pages = ((uint64_t)desc_len + 4095) / 4096;
    uint64_t desc_phys  = 0;
    void    *desc_virt  = usb_alloc_dma_pages(desc_pages, &desc_phys);
    if (!desc_virt) {
        return false;
    }

    memset(dev, 0, sizeof(HidDevice));
    dev->iface            = iface;
    dev->ep_addr          = ep_addr;
    dev->report_desc_virt = (uint8_t *)desc_virt;
    dev->report_desc_phys = desc_phys;
    dev->report_desc_len  = desc_len;

    if (!hid_device_fetch_report_descriptor(dev)) {
        usb_free_dma_pages(desc_virt, desc_pages);
        return false;
    }

    HidParser parser;
    hid_parser_init(&parser, dev->report_desc_virt, dev->report_desc_len);
    HidDescriptor desc;
    if (!hid_parser_parse(&parser, &desc)) {
        kerror("HID: Failed to parse descriptor");
        usb_free_dma_pages(desc_virt, desc_pages);
        return false;
    }
    dev->descriptor = desc;

    uint32_t max_report_size = 0;
    for (uint32_t i = 0; i < 256; i++) {
        if (!dev->descriptor.reports.used[i]) {
            continue;
        }
        HidReport *report = &dev->descriptor.reports.values[i];
        uint32_t   bytes  = hid_report_size_bytes(report, HID_KIND_INPUT);
        if (bytes > max_report_size) {
            max_report_size = bytes;
        }
    }

    uint64_t pages_needed = ((uint64_t)max_report_size + 4095) / 4096;
    uint64_t buf_phys     = 0;
    void    *buf_virt     = usb_alloc_dma_pages(pages_needed, &buf_phys);

    dev->buf_virt        = (uint8_t *)buf_virt;
    dev->buf_phys        = buf_phys;
    dev->max_report_size = (uint16_t)max_report_size;

    hid_device_set_protocol(dev, USB_PROTO_REPORT);
    hid_device_set_idle(dev, 0);

    return true;
}

void hid_device_free(HidDevice *dev) {
    hid_descriptor_free(&dev->descriptor);

    if (dev->buf_virt) {
        usb_free_dma_pages(dev->buf_virt, 1);
        dev->buf_virt = NULL;
    }
    if (dev->report_desc_virt) {
        uint64_t pages = ((uint64_t)dev->report_desc_len + 4095) / 4096;
        usb_free_dma_pages(dev->report_desc_virt, pages);
        dev->report_desc_virt = NULL;
    }
}

void hid_device_submit_transfer(HidDevice *dev) {
    if (!usb_device_submit_transfer(dev->iface->device, (GeneralTransferArgs){
            .ep_addr     = dev->ep_addr,
            .buffer_phys = dev->buf_phys,
            .length      = dev->max_report_size,
        })) {
        kerror("HID: Submit transfer failed");
    }
}

void hid_device_set_protocol(HidDevice *dev, uint16_t proto) {
    if (!usb_device_submit_control(dev->iface->device,
                                   (ControlTransferArgs){
                                       .setup = (SetupPacket){
                                           .request_type = USB_REQ_TYPE_CLASS |
                                                           USB_REQ_REC_INTERFACE,
                                           .request = USB_REQ_SET_PROTOCOL,
                                           .value   = proto,
                                           .index   = dev->iface->desc.interface_number,
                                           .length  = 0,
                                       },
                                       .buffer_phys = 0,
                                   })) {
        kwarn("HID: Set protocol failed (ignored)");
    }
}

void hid_device_set_idle(HidDevice *dev, uint16_t duration) {
    if (!usb_device_submit_control(dev->iface->device,
                                   (ControlTransferArgs){
                                       .setup = (SetupPacket){
                                           .request_type = USB_REQ_TYPE_CLASS |
                                                           USB_REQ_REC_INTERFACE,
                                           .request = USB_REQ_SET_IDLE,
                                           .value   = duration,
                                           .index   = dev->iface->desc.interface_number,
                                           .length  = 0,
                                       },
                                       .buffer_phys = 0,
                                   })) {
        kwarn("HID: Set idle failed (ignored)");
    }
}

bool hid_device_fetch_report_descriptor(HidDevice *dev) {
    if (!usb_device_submit_control(dev->iface->device,
                                   (ControlTransferArgs){
                                       .setup = (SetupPacket){
                                           .request_type = USB_REQ_DIR_IN |
                                                           USB_REQ_REC_INTERFACE,
                                           .request = USB_REQ_GET_DESCRIPTOR,
                                           .value   = (uint16_t)((USB_DESC_REPORT << 8) | 0),
                                           .index   = dev->iface->desc.interface_number,
                                           .length  = dev->report_desc_len,
                                       },
                                       .buffer_phys = dev->report_desc_phys,
                                   })) {
        kerror("HID: Failed to fetch report descriptor");
        return false;
    }
    return true;
}
