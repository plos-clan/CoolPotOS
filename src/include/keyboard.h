#pragma once

#define PS2_CMD_PORT 0x64
#define PS2_DATA_PORT 0x60

#define KB_STATUS_IBF 0x02
#define KB_STATUS_OBF 0x01
#define KB_INIT_MODE 0x47

#define KB_EN_MOUSE_INTFACE 0xa8
#define KB_SEND2MOUSE 0xd4
#define MOUSE_EN 0xf4

#include "ctype.h"

typedef enum mouse_type {
    Standard = 0,
    OnlyScroll = 1,
    FiveButton = 2,
}MouseType;

typedef struct{
    uint8_t buf[4], phase;
    int x, y, btn;
    char roll;
    bool left;
    bool center;
    bool right;
    int scroll;
} mouse_dec;

void keyboard_setup();
void mouse_setup();

int kernel_getch();
int input_char_inSM(); //获取解析前的扫描码
