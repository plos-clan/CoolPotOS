#pragma once

#define INPUT_KEYBOARD_ID 1
#define INPUT_MOUSE_ID    2

#define EV_PRESS   0
#define EV_RELEASE 1

#include "llist_queue.h"
#include "types.h"

typedef struct input_device  indev_t;
typedef struct input_handler input_handler_t;
typedef enum input_type      intype;

typedef void (*input_disconnect)(indev_t *device);
typedef void (*input_connect)(indev_t *device);
typedef void (*input_event_handle)(indev_t *device, intype type, uint64_t code, uint8_t value);

enum input_type {
    EV_CHAR, // 转义字符
    EV_KEY,  // 按键扫描码
    EV_REL,  // 相对位移
    EV_MSC,  // 其他
};

struct input_handler {
    input_event_handle handle;
    input_disconnect   disconnect;
    input_connect      connect;
    uint64_t           id;    //设备匹配ID
    size_t             index; // 列表索引
};

struct input_device {
    char         *name;     // 设备名
    void         *handler;  // 设备句柄
    uint64_t      id;       // 设备匹配ID
    size_t        index;    // 注册索引
    list_queue_t *handlers; // 处理器
};

indev_t *alloc_input_dev();
errno_t  register_input_device(indev_t *device);
errno_t  register_input_handler(input_handler_t *handler);
errno_t  delete_input_device(indev_t *device);
void     send_input_event(indev_t *dev, intype type, uint64_t code, uint8_t value);
void     init_input_manager();
