#pragma once

#define MOUSE_IRQ    12
#define KB_DATA  0x60
#define KB_CMD   0x64
#define KB_ACK   0xfa
#define LED_CODE 0xed
#define MOUSE_ROLL_UP 1
#define MOUSE_ROLL_DOWN 2
#define MOUSE_ROLL_NONE 0in

#include "ctypes.h"

typedef struct {
    uint8_t buf[3], phase;
    int x, y, btn;
    char roll;
    bool left;
    bool center;
    bool right;
} MOUSE_DEC;

int get_mouse_x();
int get_mouse_y();
void mouse_init();