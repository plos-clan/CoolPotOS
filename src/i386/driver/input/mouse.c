/*
 * OpenXJ380 Mouse Driver
 * Copyright 2024 by XingJiStudio
 */
#include "mouse.h"
#include "isr.h"
#include "io.h"
#include "klog.h"
#include "video.h"

MOUSE_DEC mdec;

int mouse_x;
int mouse_y;

static void kb_wait() {
    uint8_t kb_stat;
    do {
        kb_stat = io_in8(KB_CMD);
    } while (kb_stat & 0x02);
}

static bool mouse_decode(uint8_t data) {
    if (mdec.phase == 0) {
        if (data == 0xfa) {
            mdec.phase = 1;
        }
        return false;
    }
    if (mdec.phase == 1) {
        if ((data & 0xc8) == 0x08) {
            mdec.buf[0] = data;
            mdec.phase = 2;
        }
        return 0;
    }
    if (mdec.phase == 2) {
        mdec.buf[1] = data;
        mdec.phase = 3;
        return 0;
    }
    if (mdec.phase == 3) {
        mdec.buf[2] = data;
        mdec.phase = 1;
        mdec.btn = mdec.buf[0] & 0x07;
        mdec.x = mdec.buf[1];
        mdec.y = mdec.buf[2];

        if ((mdec.buf[0] & 0x10) != 0) mdec.x |= 0xffffff00;
        if ((mdec.buf[0] & 0x20) != 0) mdec.y |= 0xffffff00;
        mdec.y = -mdec.y;
        return true;
    }
    return false;
}

static void mouse_handler(registers_t *reg) {
    uint8_t data = io_in8(KB_DATA);

    if (mouse_decode(data)) {
        mouse_x += mdec.x;
        mouse_y += mdec.y;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > get_vbe_width() - 1) mouse_x = get_vbe_width() - 1;
        if (mouse_y > get_vbe_height() - 1) mouse_y = get_vbe_height() - 1;

        mdec.left = false;
        mdec.center = false;
        mdec.right = false;

        if(mdec.btn & 0x01){
            mdec.left = true;
        }
        if(mdec.btn & 0x02){
            mdec.right = true;
        }
        if(mdec.btn & 0x04) {
            mdec.center = true;
        }
    }
}

int get_mouse_x() {
    return mouse_x;
}

int get_mouse_y() {
    return mouse_y;
}

void mouse_init() {
    kb_wait();
    io_out8(KB_CMD, 0x60);
    kb_wait();
    io_out8(KB_DATA, 0x47);

    kb_wait();
    io_out8(KB_CMD, 0xd4);
    kb_wait();
    io_out8(KB_DATA, 0xf4);

    register_interrupt_handler(0x20 + MOUSE_IRQ, mouse_handler);

    mouse_x = (get_vbe_width() - 16) / 2;
    mouse_y = (get_vbe_height() - 28 - 29) / 2;
    mdec.phase = 0;

    klogf(true, "Load PS/2 Mouse device.\n");
}