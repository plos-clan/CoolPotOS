#pragma once

// USB HID Class 定义
#define USB_CLASS_HID 0x03

// HID Subclass
#define HID_SUBCLASS_NONE 0x00
#define HID_SUBCLASS_BOOT 0x01

// HID Protocol
#define HID_PROTOCOL_NONE     0x00
#define HID_PROTOCOL_KEYBOARD 0x01
#define HID_PROTOCOL_MOUSE    0x02

// HID Descriptor Types
#define HID_DT_HID      0x21
#define HID_DT_REPORT   0x22
#define HID_DT_PHYSICAL 0x23

// HID Class Requests
#define HID_REQ_GET_REPORT   0x01
#define HID_REQ_GET_IDLE     0x02
#define HID_REQ_GET_PROTOCOL 0x03
#define HID_REQ_SET_REPORT   0x09
#define HID_REQ_SET_IDLE     0x0A
#define HID_REQ_SET_PROTOCOL 0x0B

// HID Report Types
#define HID_REPORT_TYPE_INPUT   0x01
#define HID_REPORT_TYPE_OUTPUT  0x02
#define HID_REPORT_TYPE_FEATURE 0x03

// HID Protocol
#define HID_PROTOCOL_BOOT   0x00
#define HID_PROTOCOL_REPORT 0x01

// 键盘修饰键位
#define HID_KEY_MOD_LCTRL  0x01
#define HID_KEY_MOD_LSHIFT 0x02
#define HID_KEY_MOD_LALT   0x04
#define HID_KEY_MOD_LMETA  0x08
#define HID_KEY_MOD_RCTRL  0x10
#define HID_KEY_MOD_RSHIFT 0x20
#define HID_KEY_MOD_RALT   0x40
#define HID_KEY_MOD_RMETA  0x80

// 鼠标按键位
#define HID_MOUSE_BTN_LEFT   0x01
#define HID_MOUSE_BTN_RIGHT  0x02
#define HID_MOUSE_BTN_MIDDLE 0x04

#include "driver_subsystem.h"

// HID 描述符
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bDescriptorType2;  // Report descriptor type
    uint16_t wDescriptorLength; // Report descriptor length
} __attribute__((packed)) hid_descriptor_t;

// Boot Protocol 键盘报告
typedef struct {
    uint8_t modifiers; // 修饰键
    uint8_t reserved;  // 保留
    uint8_t keys[6];   // 按键码数组
} __attribute__((packed)) hid_keyboard_report_t;

// Boot Protocol 鼠标报告
typedef struct {
    uint8_t buttons; // 按键状态
    int8_t  x;       // X 轴移动
    int8_t  y;       // Y 轴移动
    int8_t  wheel;   // 滚轮（可选）
} __attribute__((packed)) hid_mouse_report_t;

// HID 设备类型
typedef enum {
    HID_TYPE_UNKNOWN = 0,
    HID_TYPE_KEYBOARD,
    HID_TYPE_MOUSE,
    HID_TYPE_GENERIC
} hid_device_type_t;

// HID 事件类型
typedef enum {
    HID_EVENT_KEY_PRESS,
    HID_EVENT_KEY_RELEASE,
    HID_EVENT_MOUSE_MOVE,
    HID_EVENT_MOUSE_BUTTON
} hid_event_type_t;

// HID 键盘事件
typedef struct {
    uint8_t keycode;
    uint8_t modifiers;
    bool    pressed;
} hid_key_event_t;

// HID 鼠标事件
typedef struct {
    int16_t x;
    int16_t y;
    int8_t  wheel;
    uint8_t buttons;
} hid_mouse_event_t;

// HID 事件
typedef struct {
    hid_event_type_t type;
    union {
        hid_key_event_t   key;
        hid_mouse_event_t mouse;
    };
} hid_event_t;

// HID 事件回调
typedef void (*hid_event_callback_t)(hid_event_t *event, void *user_data);

// HID 设备结构
typedef struct usb_hid_device {
    usb_device_t *usb_device;

    uint8_t  interface_number;
    uint8_t  interrupt_in_ep;
    uint16_t interrupt_in_max_packet;
    uint8_t  interrupt_interval;

    hid_device_type_t device_type;
    uint8_t           protocol;

    hid_descriptor_t hid_desc;

    // 键盘状态
    hid_keyboard_report_t keyboard_report;
    hid_keyboard_report_t prev_keyboard_report;

    // 鼠标状态
    hid_mouse_report_t mouse_report;
    hid_mouse_report_t prev_mouse_report;

    // 输入缓冲区
    uint8_t *input_buffer;
    uint32_t input_buffer_size;

    // 中断传输
    usb_transfer_t *interrupt_transfer;
    bool            transfer_active;

    // 事件回调
    hid_event_callback_t event_callback;
    void                *callback_user_data;

    struct usb_hid_device *next;
} usb_hid_device_t;

// HID 驱动 API
int  usb_hid_init(void);
int  usb_hid_probe(usb_device_t *device);
void usb_hid_remove(usb_hid_device_t *hid);

// 设置事件回调
void usb_hid_set_event_callback(usb_hid_device_t *hid, hid_event_callback_t callback,
                                void *user_data);

// 获取 HID 设备
usb_hid_device_t *usb_hid_get_device(int index);
usb_hid_device_t *usb_hid_get_keyboard(void);
usb_hid_device_t *usb_hid_get_mouse(void);

// HID 控制
int hid_set_protocol(usb_hid_device_t *hid, uint8_t protocol);
int hid_set_idle(usb_hid_device_t *hid, uint8_t duration);
int hid_get_report(usb_hid_device_t *hid, uint8_t type, uint8_t id, void *data, uint16_t length);

// 键码转换
const char *hid_keycode_to_string(uint8_t keycode);
char        hid_keycode_to_ascii(uint8_t keycode, uint8_t modifiers);
