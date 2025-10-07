#pragma once

#include "usb.h"

typedef enum {
    USB_ENUM_STATE_IDLE = 0,
    USB_ENUM_STATE_RESET,
    USB_ENUM_STATE_RESET_WAIT,
    USB_ENUM_STATE_GET_DESC_8,
    USB_ENUM_STATE_ADDRESS,
    USB_ENUM_STATE_ADDRESS_WAIT,
    USB_ENUM_STATE_GET_DEVICE_DESC,
    USB_ENUM_STATE_GET_CONFIG_DESC_HEADER,
    USB_ENUM_STATE_GET_CONFIG_DESC_FULL,
    USB_ENUM_STATE_SET_CONFIG,
    USB_ENUM_STATE_CONFIGURE_ENDPOINTS,
    USB_ENUM_STATE_CONFIGURED,
    USB_ENUM_STATE_ERROR,
} usb_enum_state_t;

// 枚举上下文
typedef struct {
    usb_device_t    *device;
    usb_hcd_t       *hcd;
    uint8_t          port_id;
    uint8_t          address;
    usb_enum_state_t state;

    uint8_t *buffer;
    uint32_t buffer_size;

    int      retry_count;
    uint32_t timestamp;
} usb_enum_context_t;

// 枚举函数
int usb_enumerate_device(usb_hcd_t *hcd, uint8_t port_id, uint8_t speed);
int usb_enum_state_machine(usb_enum_context_t *ctx);

// 辅助函数
int usb_parse_config_descriptor(usb_device_t *device, uint8_t *buffer, uint32_t length);
int usb_configure_device_endpoints(usb_device_t *device);