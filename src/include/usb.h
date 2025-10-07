#pragma once

#define MAX_USB_DRIVERS_NUM 32

// USB标准定义
#define USB_DIR_OUT 0
#define USB_DIR_IN  0x80

#define USB_TYPE_STANDARD (0x00 << 5)
#define USB_TYPE_CLASS    (0x01 << 5)
#define USB_TYPE_VENDOR   (0x02 << 5)

#define USB_RECIP_DEVICE    0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT  0x02

// USB标准请求
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B

// 描述符类型
#define USB_DT_DEVICE    0x01
#define USB_DT_CONFIG    0x02
#define USB_DT_STRING    0x03
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT  0x05

// 端点类型
#define USB_ENDPOINT_XFER_CONTROL 0
#define USB_ENDPOINT_XFER_ISOC    1
#define USB_ENDPOINT_XFER_BULK    2
#define USB_ENDPOINT_XFER_INT     3

// 设备速度
#define USB_SPEED_UNKNOWN 0
#define USB_SPEED_LOW     1
#define USB_SPEED_FULL    2
#define USB_SPEED_HIGH    3
#define USB_SPEED_SUPER   4

// USB设备状态
#define USB_STATE_NOTATTACHED 0
#define USB_STATE_ATTACHED    1
#define USB_STATE_POWERED     2
#define USB_STATE_DEFAULT     3
#define USB_STATE_ADDRESS     4
#define USB_STATE_CONFIGURED  5
#define USB_STATE_SUSPENDED   6

#include "cptype.h"

// USB标准设备请求
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_device_request_t;

// USB设备描述符
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

// USB配置描述符
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

// USB接口描述符
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

// USB端点描述符
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

// 前向声明
typedef struct usb_device   usb_device_t;
typedef struct usb_endpoint usb_endpoint_t;
typedef struct usb_transfer usb_transfer_t;
typedef struct usb_hcd      usb_hcd_t;

// USB传输完成回调
typedef void (*usb_transfer_callback_t)(usb_transfer_t *transfer);

// USB传输结构
struct usb_transfer {
    usb_device_t   *device;
    usb_endpoint_t *endpoint;

    void    *buffer;
    uint32_t length;
    uint32_t actual_length;

    int   status;
    void *hcd_private; // HCD私有数据

    usb_transfer_callback_t callback;
    void                   *user_data;

    struct usb_transfer *next;
};

// USB端点结构
struct usb_endpoint {
    uint8_t  address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;

    usb_device_t *device;
    void         *hcd_private;
};

// USB设备结构
struct usb_device {
    uint8_t port;

    uint8_t address;
    uint8_t speed;
    uint8_t state;

    usb_device_descriptor_t  descriptor;
    usb_config_descriptor_t *config_descriptor;

    uint8_t class;
    uint8_t subclass;

    usb_endpoint_t endpoints[32]; // 最多16 IN + 16 OUT

    usb_hcd_t *hcd;
    void      *hcd_private; // HCD私有数据

    void *private_data;

    struct usb_device *next;
};

// USB主机控制器驱动操作
typedef struct {
    int (*init)(usb_hcd_t *hcd);
    int (*shutdown)(usb_hcd_t *hcd);

    int (*reset_port)(usb_hcd_t *hcd, uint8_t port);
    int (*enable_slot)(usb_hcd_t *hcd, usb_device_t *device);
    int (*disable_slot)(usb_hcd_t *hcd, usb_device_t *device);

    int (*address_device)(usb_hcd_t *hcd, usb_device_t *device, uint8_t address);
    int (*configure_endpoint)(usb_hcd_t *hcd, usb_endpoint_t *endpoint);

    int (*control_transfer)(usb_hcd_t *hcd, usb_transfer_t *transfer, usb_device_request_t *setup);
    int (*bulk_transfer)(usb_hcd_t *hcd, usb_transfer_t *transfer);
    int (*interrupt_transfer)(usb_hcd_t *hcd, usb_transfer_t *transfer);
} usb_hcd_ops_t;

// USB主机控制器驱动
struct usb_hcd {
    const char    *name;
    usb_hcd_ops_t *ops;

    void *regs; // 寄存器基址
    void *private_data;

    usb_device_t *devices;
    uint8_t       num_ports;

    struct usb_hcd *next;
};

typedef struct usb_driver {
    uint8_t class;
    uint8_t subclass;
    int (*probe)(usb_device_t *usbdev);
    int (*remove)(usb_device_t *usbdev);
} usb_driver_t;

void register_usb_driver(usb_driver_t *driver);

// USB核心API
int        usb_init(void);
usb_hcd_t *usb_register_hcd(const char *name, usb_hcd_ops_t *ops, void *regs, void *data);
void       usb_unregister_hcd(usb_hcd_t *hcd);

usb_device_t *usb_alloc_device(usb_hcd_t *hcd);
void          usb_free_device(usb_device_t *device);
int           usb_add_device(usb_device_t *device);
void          usb_remove_device(usb_device_t *device);

usb_transfer_t *usb_alloc_transfer(void);
void            usb_free_transfer(usb_transfer_t *transfer);

int usb_control_transfer(usb_device_t *device, usb_device_request_t *setup, void *data,
                         uint32_t length, usb_transfer_callback_t callback, void *user_data);

int usb_bulk_transfer(usb_device_t *device, uint8_t endpoint, void *data, uint32_t length,
                      usb_transfer_callback_t callback, void *user_data);

int usb_interrupt_transfer(usb_device_t *device, uint8_t endpoint, void *data, uint32_t length,
                           usb_transfer_callback_t callback, void *user_data);

// 辅助函数
int usb_get_descriptor(usb_device_t *device, uint8_t type, uint8_t index, void *buffer,
                       uint16_t length);
int usb_set_address(usb_device_t *device, uint8_t address);
int usb_set_configuration(usb_device_t *device, uint8_t config);
