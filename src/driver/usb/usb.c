#include "usb.h"
#include "heap.h"
#include "kprint.h"
#include "krlibc.h"
#include "klog.h"

usb_driver_t *usb_drivers[MAX_USB_DRIVERS_NUM];

static struct {
    usb_hcd_t      *hcd_list;
    usb_transfer_t *transfer_pool;
} usb_context;

int usb_init(void) {
    memset(&usb_context, 0, sizeof(usb_context));
    kinfo("USB Core initialized");
    return 0;
}

// 注册主机控制器驱动
usb_hcd_t *usb_register_hcd(const char *name, usb_hcd_ops_t *ops, void *regs, void *data) {
    usb_hcd_t *hcd = (usb_hcd_t *)malloc(sizeof(usb_hcd_t));
    if (!hcd) { return NULL; }

    memset(hcd, 0, sizeof(usb_hcd_t));
    hcd->name         = name;
    hcd->ops          = ops;
    hcd->regs         = regs;
    hcd->private_data = data;

    // 初始化HCD
    if (hcd->ops->init) {
        if (hcd->ops->init(hcd) != 0) {
            free(hcd);
            return NULL;
        }
    }

    // 添加到HCD列表
    hcd->next            = usb_context.hcd_list;
    usb_context.hcd_list = hcd;

    printk("USB HCD \"%s\" registered\n", name);
    return hcd;
}

// 注销主机控制器驱动
void usb_unregister_hcd(usb_hcd_t *hcd) {
    if (!hcd) return;

    if (hcd->ops->shutdown) { hcd->ops->shutdown(hcd); }

    // 从列表中移除
    usb_hcd_t **prev = &usb_context.hcd_list;
    while (*prev) {
        if (*prev == hcd) {
            *prev = hcd->next;
            break;
        }
        prev = &(*prev)->next;
    }

    free(hcd);
}

// 分配USB设备
usb_device_t *usb_alloc_device(usb_hcd_t *hcd) {
    usb_device_t *device = (usb_device_t *)malloc(sizeof(usb_device_t));
    if (!device) { return NULL; }

    memset(device, 0, sizeof(usb_device_t));
    device->hcd   = hcd;
    device->state = USB_STATE_ATTACHED;

    return device;
}

// 释放USB设备
void usb_free_device(usb_device_t *device) {
    if (!device) return;

    if (device->config_descriptor) { free(device->config_descriptor); }

    free(device);
}

usb_driver_t *usb_find_driver(usb_device_t *device) {
    for (int i = 0; i < MAX_USB_DRIVERS_NUM; i++) {
        if (usb_drivers[i]) {
            if (usb_drivers[i]->class == device->class) { return usb_drivers[i]; }
        }
    }

    return NULL;
}

// 添加USB设备
int usb_add_device(usb_device_t *device) {
    if (!device || !device->hcd) { return -1; }

    // 添加到HCD的设备列表
    device->next         = device->hcd->devices;
    device->hcd->devices = device;

    logkf("usb: device added: addr=%d, speed=%d\n", device->address, device->speed);

    usb_driver_t *driver = usb_find_driver(device);
    if (!driver || driver->probe(device)) return -1;

    return 0;
}

// 移除USB设备
void usb_remove_device(usb_device_t *device) {
    if (!device || !device->hcd) return;

    // 从HCD设备列表中移除
    usb_device_t **prev = &device->hcd->devices;
    while (*prev) {
        if (*prev == device) {
            *prev = device->next;
            break;
        }
        prev = &(*prev)->next;
    }

    usb_free_device(device);
}

// 分配USB传输
usb_transfer_t *usb_alloc_transfer(void) {
    usb_transfer_t *transfer = (usb_transfer_t *)malloc(sizeof(usb_transfer_t));
    if (!transfer) { return NULL; }

    memset(transfer, 0, sizeof(usb_transfer_t));
    return transfer;
}

// 释放USB传输
void usb_free_transfer(usb_transfer_t *transfer) {
    if (transfer) { free(transfer); }
}

// 控制传输
int usb_control_transfer(usb_device_t *device, usb_device_request_t *setup, void *data,
                         uint32_t length, usb_transfer_callback_t callback, void *user_data) {
    if (!device || !device->hcd || !device->hcd->ops->control_transfer) { return -1; }

    usb_transfer_t *transfer = usb_alloc_transfer();
    if (!transfer) { return -1; }

    transfer->device    = device;
    transfer->endpoint  = &device->endpoints[0]; // 控制端点总是0
    transfer->buffer    = data;
    transfer->length    = length;
    transfer->callback  = callback;
    transfer->user_data = user_data;

    return device->hcd->ops->control_transfer(device->hcd, transfer, setup);
}

// 批量传输
int usb_bulk_transfer(usb_device_t *device, uint8_t endpoint_addr, void *data, uint32_t length,
                      usb_transfer_callback_t callback, void *user_data) {
    if (!device || !device->hcd || !device->hcd->ops->bulk_transfer) { return -1; }

    // 查找端点
    usb_endpoint_t *endpoint = NULL;
    for (int i = 0; i < 32; i++) {
        if (device->endpoints[i].address == endpoint_addr) {
            endpoint = &device->endpoints[i];
            break;
        }
    }

    if (!endpoint) { return -1; }

    usb_transfer_t *transfer = usb_alloc_transfer();
    if (!transfer) { return -1; }

    transfer->device    = device;
    transfer->endpoint  = endpoint;
    transfer->buffer    = data;
    transfer->length    = length;
    transfer->callback  = callback;
    transfer->user_data = user_data;

    return device->hcd->ops->bulk_transfer(device->hcd, transfer);
}

// 中断传输
int usb_interrupt_transfer(usb_device_t *device, uint8_t endpoint_addr, void *data, uint32_t length,
                           usb_transfer_callback_t callback, void *user_data) {
    if (!device || !device->hcd || !device->hcd->ops->interrupt_transfer) { return -1; }

    // 查找端点
    usb_endpoint_t *endpoint = NULL;
    for (int i = 0; i < 32; i++) {
        if (device->endpoints[i].address == endpoint_addr) {
            endpoint = &device->endpoints[i];
            break;
        }
    }

    if (!endpoint) { return -1; }

    usb_transfer_t *transfer = usb_alloc_transfer();
    if (!transfer) { return -1; }

    transfer->device    = device;
    transfer->endpoint  = endpoint;
    transfer->buffer    = data;
    transfer->length    = length;
    transfer->callback  = callback;
    transfer->user_data = user_data;

    return device->hcd->ops->interrupt_transfer(device->hcd, transfer);
}

// 获取描述符
int usb_get_descriptor(usb_device_t *device, uint8_t type, uint8_t index, void *buffer,
                       uint16_t length) {
    usb_device_request_t setup = {.bmRequestType =
                                      USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                  .bRequest = USB_REQ_GET_DESCRIPTOR,
                                  .wValue   = (type << 8) | index,
                                  .wIndex   = 0,
                                  .wLength  = length};

    return usb_control_transfer(device, &setup, buffer, length, NULL, NULL);
}

// 设置地址
int usb_set_address(usb_device_t *device, uint8_t address) {
    usb_device_request_t setup = {.bmRequestType =
                                      USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                  .bRequest = USB_REQ_SET_ADDRESS,
                                  .wValue   = address,
                                  .wIndex   = 0,
                                  .wLength  = 0};

    int ret = usb_control_transfer(device, &setup, NULL, 0, NULL, NULL);
    if (ret == 0) {
        device->address = address;
        device->state   = USB_STATE_ADDRESS;
    }

    return ret;
}

// 设置配置
int usb_set_configuration(usb_device_t *device, uint8_t config) {
    usb_device_request_t setup = {.bmRequestType =
                                      USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                  .bRequest = USB_REQ_SET_CONFIGURATION,
                                  .wValue   = config,
                                  .wIndex   = 0,
                                  .wLength  = 0};

    int ret = usb_control_transfer(device, &setup, NULL, 0, NULL, NULL);
    if (ret == 0) { device->state = USB_STATE_CONFIGURED; }

    return ret;
}

void register_usb_driver(usb_driver_t *driver) {
    for (int i = 0; i < MAX_USB_DRIVERS_NUM; i++) {
        if (!usb_drivers[i]) {
            usb_drivers[i] = driver;
            break;
        }
    }
}
