#pragma once

#define PS2K_PNP_ID "PNP0303"

#define SCANCODE_ENTER   28
#define SCANCODE_BACK    14
#define SCANCODE_SHIFT_L 42
#define SCANCODE_SHIFT_R 0x36
#define SCANCODE_CAPS    58
#define SCANCODE_UP      0x48

#define CHARACTER_ENTER '\n'
#define CHARACTER_BACK  '\b'

#include "types.h"

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

void ps2_kdb_setup();
