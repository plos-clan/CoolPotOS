#include "gop.h"
#include "io.h"
#include "isr.h"
#include "keyboard.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "terminal.h"

MouseType  type;
mouse_dec  mouse_decode;
size_t     mouse_x;
size_t     mouse_y;
ticketlock mouse_lock;

bool mousedecode(uint8_t data) {
    if (mouse_decode.phase == 0) {
        if (data == 0xfa) { mouse_decode.phase = 1; }
        return false;
    }
    if (mouse_decode.phase == 1) {
        if ((data & 0xc8) == 0x08) {
            mouse_decode.buf[0] = data;
            mouse_decode.phase  = 2;
        }
        return 0;
    }
    if (mouse_decode.phase == 2) {
        mouse_decode.buf[1] = data;
        mouse_decode.phase  = 3;
        return 0;
    }
    if (mouse_decode.phase == 3) {
        mouse_decode.buf[2] = data;
        mouse_decode.phase  = 1;
        mouse_decode.btn    = mouse_decode.buf[0] & 0x07;
        mouse_decode.x      = mouse_decode.buf[1];
        mouse_decode.y      = mouse_decode.buf[2];

        if ((mouse_decode.buf[0] & 0x10) != 0) mouse_decode.x |= 0xffffff00;
        if ((mouse_decode.buf[0] & 0x20) != 0) mouse_decode.y |= 0xffffff00;
        mouse_decode.y = -mouse_decode.y;
        if (type == OnlyScroll || type == FiveButton) {
            mouse_decode.phase = 4;
            return 0;
        } else
            return true;
    }
    if (mouse_decode.phase == 4) {
        mouse_decode.buf[3] = data;
        mouse_decode.phase  = 1;
        int wheel_delta     = mouse_decode.buf[3];
        if (wheel_delta >= 255) wheel_delta = -1;

        mouse_decode.scroll = -wheel_delta;
        return true;
    }
    return false;
}

__IRQHANDLER void mouse_handle(interrupt_frame_t *frame) {
    UNUSED(frame);
    send_eoi();
    ticket_lock(&mouse_lock);
    uint8_t data = io_in8(PS2_DATA_PORT);
    if (mousedecode(data)) {
        mouse_x += mouse_decode.x;
        mouse_y += mouse_decode.y;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > get_screen_width() - 1) mouse_x = get_screen_width() - 1;
        if (mouse_y > get_screen_height() - 1) mouse_y = get_screen_height() - 1;

        mouse_decode.left   = false;
        mouse_decode.center = false;
        mouse_decode.right  = false;

        if (mouse_decode.btn & 0x01) { mouse_decode.left = true; }
        if (mouse_decode.btn & 0x02) { mouse_decode.right = true; }
        if (mouse_decode.btn & 0x04) { mouse_decode.center = true; }

        terminal_handle_mouse_scroll(mouse_decode.scroll);

        // 不知道会不会死锁不过大概率会（

        //        logkf("mouse x:%d y:%d right:%s left:%s center: %s scroll: %d\n",mouse_x,mouse_y,
        //              mouse_decode.right ? "true" : "false",
        //              mouse_decode.left ? "true" : "false",
        //              mouse_decode.center ? "true" : "false",
        //              mouse_decode.scroll
        //        );
    }
    ticket_unlock(&mouse_lock);
}

static inline void wait_for_read() {
    while (!(io_in8(PS2_CMD_PORT) & KB_STATUS_OBF))
        ;
}

static inline void wait_for_write() {
    while (io_in8(PS2_CMD_PORT) & KB_STATUS_IBF)
        ;
}

static bool send_command(uint8_t value) {
    wait_for_write();
    io_out8(PS2_CMD_PORT, KB_SEND2MOUSE);
    wait_for_write();
    io_out8(PS2_DATA_PORT, value);
    return io_in8(PS2_DATA_PORT) != 0xfa;
}

static MouseType get_mouse_type() {
    send_command(0xf3);
    send_command(200);

    send_command(0xf3);
    send_command(100);

    send_command(0xf3);
    send_command(80);

    send_command(0xf2);
    wait_for_read();
    uint8_t type0 = io_in8(PS2_DATA_PORT);
    return type0 == 0x3 ? OnlyScroll : (type0 == 0x4 ? FiveButton : Standard);
}

void mouse_setup() {
    register_interrupt_handler(mouse, (void *)mouse_handle, 0, 0x8E);
    send_command(MOUSE_EN);
    type = get_mouse_type();
    kinfo("Setup PS/2 %s mouse.",
          type == OnlyScroll ? "OnlyScroll" : (type == FiveButton ? "FiveButton" : "Standard"));
}
