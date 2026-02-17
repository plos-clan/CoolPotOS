#include "driver/usb/bus/device.h"
#include "driver/usb/defs/defs.h"
#include "driver/usb/usb_mem.h"
#include "krlibc.h"
#include "term/klog.h"

static void
usb_device_parse_config_tree(UsbDevice *dev, const uint8_t *config_raw, uint16_t total_len);
static void usb_device_parse_interface_descriptor(UsbDevice *dev, const uint8_t *ptr);
static void usb_device_parse_hid_descriptor(UsbDevice *dev, const uint8_t *ptr);
static void usb_device_parse_endpoint_descriptor(UsbDevice *dev, const uint8_t *ptr);
static void usb_device_parse_ss_companion(UsbDevice *dev, const uint8_t *ptr);

bool usb_device_enumerate(UsbDevice *dev) {
    if (!dev) {
        return false;
    }

    uint64_t desc_phys = 0;
    void *desc_virt    = usb_alloc_dma_pages(1, &desc_phys);
    if (!desc_virt) {
        return false;
    }

    if (!usb_device_submit_control(
            dev,
            (ControlTransferArgs){
                .setup =
                    (SetupPacket){
                                  .request_type = USB_REQ_DIR_IN,
                                  .request      = USB_REQ_GET_DESCRIPTOR,
                                  .value        = (uint16_t)((USB_DESC_DEVICE << 8) | 0),
                                  .index        = 0,
                                  .length       = (uint16_t)sizeof(DeviceDescriptor),
                                  },
                .buffer_phys = desc_phys,
    }
        )) {
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    dev->desc = *(DeviceDescriptor *)desc_virt;
    kinfo("USB Device: %04x:%04x", dev->desc.id_vendor, dev->desc.id_product);

    uint64_t header_phys = 0;
    void *header_virt    = usb_alloc_dma_pages(1, &header_phys);
    if (!header_virt) {
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    if (!usb_device_submit_control(
            dev,
            (ControlTransferArgs){
                .setup =
                    (SetupPacket){
                                  .request_type = USB_REQ_DIR_IN,
                                  .request      = USB_REQ_GET_DESCRIPTOR,
                                  .value        = (uint16_t)((USB_DESC_CONFIGURATION << 8) | 0),
                                  .index        = 0,
                                  .length       = (uint16_t)sizeof(ConfigurationDescriptor),
                                  },
                .buffer_phys = header_phys,
    }
        )) {
        usb_free_dma_pages(header_virt, 1);
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    ConfigurationDescriptor *header = (ConfigurationDescriptor *)header_virt;
    uint16_t total_len              = header->total_length;
    uint8_t config_val              = header->configuration_value;

    uint64_t pages_needed = ((uint64_t)total_len + 4095) / 4096;
    uint64_t config_phys  = 0;
    void *config_virt     = usb_alloc_dma_pages(pages_needed, &config_phys);
    if (!config_virt) {
        usb_free_dma_pages(header_virt, 1);
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    if (!usb_device_submit_control(
            dev,
            (ControlTransferArgs){
                .setup =
                    (SetupPacket){
                                  .request_type = USB_REQ_DIR_IN,
                                  .request      = USB_REQ_GET_DESCRIPTOR,
                                  .value        = (uint16_t)((USB_DESC_CONFIGURATION << 8) | 0),
                                  .index        = 0,
                                  .length       = total_len,
                                  },
                .buffer_phys = config_phys,
    }
        )) {
        usb_free_dma_pages(config_virt, pages_needed);
        usb_free_dma_pages(header_virt, 1);
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    kdebug("Parsing config tree (len: %d)", total_len);
    usb_device_parse_config_tree(dev, (const uint8_t *)config_virt, total_len);

    UsbEndpointVec endpoints;
    UsbEndpointVec_init(&endpoints);

    for (size_t i = 0; i < dev->interfaces.len; i++) {
        UsbInterface *iface = &dev->interfaces.data[i];
        for (size_t j = 0; j < iface->endpoints.len; j++) {
            UsbEndpointVec_push(&endpoints, iface->endpoints.data[j]);
        }
    }

    kdebug("Configuring endpoints in hardware...");
    if (!host_configure_endpoints(&dev->host, dev->slot_id, &endpoints)) {
        UsbEndpointVec_free(&endpoints);
        usb_free_dma_pages(config_virt, pages_needed);
        usb_free_dma_pages(header_virt, 1);
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    UsbEndpointVec_free(&endpoints);

    if (!usb_device_submit_control(
            dev,
            (ControlTransferArgs){
                .setup =
                    (SetupPacket){
                                  .request_type = USB_REQ_DIR_OUT,
                                  .request      = USB_REQ_SET_CONFIGURATION,
                                  .value        = (uint16_t)config_val,
                                  .index        = 0,
                                  .length       = 0,
                                  },
                .buffer_phys = 0,
    }
        )) {
        usb_free_dma_pages(config_virt, pages_needed);
        usb_free_dma_pages(header_virt, 1);
        usb_free_dma_pages(desc_virt, 1);
        return false;
    }

    usb_device_match_drivers(dev);
    ksuccess("Device enumeration complete (slot %d)", dev->slot_id);

    usb_free_dma_pages(config_virt, pages_needed);
    usb_free_dma_pages(header_virt, 1);
    usb_free_dma_pages(desc_virt, 1);
    return true;
}

static void
usb_device_parse_config_tree(UsbDevice *dev, const uint8_t *config_raw, uint16_t total_len) {
    uint16_t offset = 0;

    while (offset < total_len) {
        const uint8_t *ptr = config_raw + offset;
        uint8_t desc_len   = ptr[0];
        uint8_t desc_type  = ptr[1];

        if ((uint16_t)(offset + desc_len) > total_len) {
            break;
        }

        switch (desc_type) {
        case USB_DESC_INTERFACE:
            usb_device_parse_interface_descriptor(dev, ptr);
            break;
        case USB_DESC_HID:
            usb_device_parse_hid_descriptor(dev, ptr);
            break;
        case USB_DESC_ENDPOINT:
            usb_device_parse_endpoint_descriptor(dev, ptr);
            break;
        case USB_DESC_SS_EP_COMPANION:
            usb_device_parse_ss_companion(dev, ptr);
            break;
        default:
            break;
        }

        offset += desc_len;
    }
}

static void usb_device_parse_interface_descriptor(UsbDevice *dev, const uint8_t *ptr) {
    const InterfaceDescriptor *desc = (const InterfaceDescriptor *)ptr;
    UsbInterface iface              = { 0 };
    iface.desc                      = *desc;
    iface.device                    = dev;

    UsbInterfaceVec_push(&dev->interfaces, iface);
}

static void usb_device_parse_hid_descriptor(UsbDevice *dev, const uint8_t *ptr) {
    UsbInterface *iface = UsbInterfaceVec_last(&dev->interfaces);
    if (!iface) {
        return;
    }

    const HidDescriptorHeader *header = (const HidDescriptorHeader *)ptr;

    uint32_t pos = sizeof(HidDescriptorHeader);
    for (uint8_t i = 0; i < header->num_descriptors; i++) {
        uint8_t desc_type = ptr[pos];
        uint16_t len_lo   = (uint16_t)ptr[pos + 1];
        uint16_t len_hi   = (uint16_t)ptr[pos + 2];
        uint16_t desc_len = len_lo | (uint16_t)(len_hi << 8);

        if (desc_type == USB_DESC_REPORT) {
            iface->extra_data.hid_report_desc_len = desc_len;
            return;
        }
        pos += 3;
    }
}

static void usb_device_parse_ss_companion(UsbDevice *dev, const uint8_t *ptr) {
    UsbInterface *iface = UsbInterfaceVec_last(&dev->interfaces);
    if (!iface) {
        return;
    }

    UsbEndpoint *ep = UsbEndpointVec_last(&iface->endpoints);
    if (!ep) {
        return;
    }

    const SsEndpointCompanionDescriptor *desc = (const SsEndpointCompanionDescriptor *)ptr;
    ep->ss_desc                               = *desc;
    ep->has_ss_desc                           = true;
}

static void usb_device_parse_endpoint_descriptor(UsbDevice *dev, const uint8_t *ptr) {
    UsbInterface *iface = UsbInterfaceVec_last(&dev->interfaces);
    if (!iface) {
        return;
    }

    const EndpointDescriptor *desc = (const EndpointDescriptor *)ptr;
    UsbEndpoint ep                 = { 0 };
    ep.desc                        = *desc;

    UsbEndpointVec_push(&iface->endpoints, ep);
    usb_endpoint_map_set(&dev->ep_map, desc->endpoint_address, (uint8_t)(dev->interfaces.len - 1));
}
