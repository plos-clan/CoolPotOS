#ifndef CPOS_KEYBOARD_H
#define CPOS_KEYBOARD_H

#include "idt.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

typedef struct {
    int is_shift;
}KEY_STATUS;

typedef struct {
    unsigned char *keyboard_map[128];
    unsigned char *shift_keyboard_map[128];
}KEY_MAP;

void init_keyboard();
int handle_keyboard_input();

#endif