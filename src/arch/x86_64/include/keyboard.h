#pragma once

#define PS2_CMD_PORT  0x64
#define PS2_DATA_PORT 0x60

#define KB_STATUS_IBF 0x02
#define KB_STATUS_OBF 0x01
#define KB_INIT_MODE  0x47

#define KBCMD_WRITE_CMD 0x60
#define KBCMD_READ_CMD  0x20

#define KB_EN_MOUSE_INTFACE 0xa8
#define KB_SEND2MOUSE       0xd4
#define MOUSE_EN            0xf4

#define SCANCODE_ENTER   28
#define SCANCODE_BACK    14
#define SCANCODE_SHIFT_L 42
#define SCANCODE_SHIFT_R 0x36
#define SCANCODE_CAPS    58
#define SCANCODE_UP      0x48

#define CHARACTER_ENTER '\n'
#define CHARACTER_BACK  '\b'

#include "cptype.h"

typedef enum mouse_type {
    Standard   = 0,
    OnlyScroll = 1,
    FiveButton = 2,
} MouseType;

typedef struct {
    uint8_t buf[4], phase;
    int     x, y, btn;
    char    roll;
    bool    left;
    bool    center;
    bool    right;
    int     scroll;
} mouse_dec;

struct keyboard_buf {
    uint8_t *p_head;
    uint8_t *p_tail;
    int32_t  count;
    bool     ctrl;
    bool     shift;
    bool     alt;
    bool     caps;
    uint8_t  buf[64];
};

enum {
    KEY_BUTTON_UP     = 0x81,
    KEY_BUTTON_DOWN   = 0x82,
    KEY_BUTTON_LEFT   = 0x83,
    KEY_BUTTON_RIGHT  = 0x84,
    KEY_BUTTON_HOME   = 0x85,
    KEY_BUTTON_DEL    = 0x86,
    KEY_BUTTON_END    = 0x87,
    KEY_BUTTON_INSERT = 0x88,
    KEY_BUTTON_PUP    = 0x89,
    KEY_BUTTON_PDOWN  = 0x8a,
};

void keyboard_setup();
void mouse_setup();

void wait_ps2_write();
void wait_ps2_read();

uint8_t keyboard_scancode(uint8_t scancode, uint8_t scancode_1, uint8_t scancode_2);
