#pragma once

#include "fs/vfs.h"
#include "lock.h"
#include "timer.h"

// Linux-compatible evdev event types
#define INPUT_EV_SYN 0x0000
#define INPUT_EV_KEY 0x0001

// Linux-compatible SYN codes
#define SYN_REPORT 0

// Linux-compatible key codes (match PS/2 Set 1 scancodes for basic keys)
#define KEY_RESERVED 0
#define KEY_ESC 1
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_Q 16
#define KEY_W 17
#define KEY_E 18
#define KEY_R 19
#define KEY_T 20
#define KEY_Y 21
#define KEY_U 22
#define KEY_I 23
#define KEY_O 24
#define KEY_P 25
#define KEY_LEFTBRACE 26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER 28
#define KEY_LEFTCTRL 29
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_F 33
#define KEY_G 34
#define KEY_H 35
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_SEMICOLON 39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE 41
#define KEY_LEFTSHIFT 42
#define KEY_BACKSLASH 43
#define KEY_Z 44
#define KEY_X 45
#define KEY_C 46
#define KEY_V 47
#define KEY_B 48
#define KEY_N 49
#define KEY_M 50
#define KEY_COMMA 51
#define KEY_DOT 52
#define KEY_SLASH 53
#define KEY_RIGHTSHIFT 54
#define KEY_KPASTERISK 55
#define KEY_LEFTALT 56
#define KEY_SPACE 57
#define KEY_CAPSLOCK 58
#define KEY_F1 59
#define KEY_F2 60
#define KEY_F3 61
#define KEY_F4 62
#define KEY_F5 63
#define KEY_F6 64
#define KEY_F7 65
#define KEY_F8 66
#define KEY_F9 67
#define KEY_F10 68
#define KEY_NUMLOCK 69
#define KEY_SCROLLLOCK 70
#define KEY_KP7 71
#define KEY_KP8 72
#define KEY_KP9 73
#define KEY_KPMINUS 74
#define KEY_KP4 75
#define KEY_KP5 76
#define KEY_KP6 77
#define KEY_KPPLUS 78
#define KEY_KP1 79
#define KEY_KP2 80
#define KEY_KP3 81
#define KEY_KP0 82
#define KEY_KPDOT 83
#define KEY_F11 87
#define KEY_F12 88

// Extended keys (0xE0 prefix)
#define KEY_KPENTER 96
#define KEY_RIGHTCTRL 97
#define KEY_KPSLASH 98
#define KEY_RIGHTALT 100
#define KEY_HOME 102
#define KEY_UP 103
#define KEY_PAGEUP 104
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_END 107
#define KEY_DOWN 108
#define KEY_PAGEDOWN 109
#define KEY_INSERT 110
#define KEY_DELETE 111

// Flag for extended scancode passed via send_input_event code parameter
#define EVDEV_EXT_FLAG 0x100

// Linux-compatible input_event structure (24 bytes on 64-bit)
struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

#define EVDEV_BUF_SIZE 256

typedef struct evdev_ctx {
    struct input_event buf[EVDEV_BUF_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    int open_count;
    spin_t lock;
} evdev_ctx_t;

// IOCTL decoding helpers (Linux _IOC encoding)
#define _IOC_NRSHIFT 0
#define _IOC_TYPESHIFT 8
#define _IOC_SIZESHIFT 16
#define _IOC_NR(nr) (((nr) >> _IOC_NRSHIFT) & 0xFF)
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & 0xFF)
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & 0x3FFF)

// Evdev ioctl type
#define EVDEV_IOC_TYPE 'E'

// Evdev ioctl numbers (nr field)
#define EVIOC_NR_GVERSION 0x01
#define EVIOC_NR_GID 0x02
#define EVIOC_NR_GNAME 0x06
#define EVIOC_NR_GPHYS 0x07
#define EVIOC_NR_GUNIQ 0x08
#define EVIOC_NR_GPROP 0x09
#define EVIOC_NR_GBIT_BASE 0x20 // EVIOCGBIT(ev) = 0x20 + ev
#define EVIOC_NR_GABS_BASE 0x40 // EVIOCGABS(abs) = 0x40 + abs

#define EV_VERSION 0x010001

// Maximum key code we support
#define KEY_MAX 111

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

#define BUS_I8042 0x11

void evdev_setup(vfs_node_t dev_root);
