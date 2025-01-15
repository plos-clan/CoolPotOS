#include "isr.h"
#include "description_table.h"
#include "krlibc.h"
#include "io.h"
#include "kprint.h"
#include "acpi.h"

static int caps_lock, shift, e0_flag = 0, ctrl = 0;
int disable_flag = 0;

char keytable[0x54] = { // 按下Shift
        0, 0x01, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q',
        'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 10, 0, 'A', 'S', 'D', 'F',
        'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',
        '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, '7', 'D', '8', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'};

char keytable1[0x54] = { // 未按下Shift
        0, 0x01, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q',
        'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 10, 0, 'a', 's', 'd', 'f',
        'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
        ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'};



__IRQHANDLER void keyboard_handler(interrupt_frame_t *frame){
    UNUSED(frame);
    io_out8(0x0020, 0x61);
    uint8_t scancode = io_in8(0x60);
    if (scancode == 0xe0) {
        e0_flag = 1;
        return;
    }
    if (scancode == 0x2a || scancode == 0x36) { // Shift按下
        shift = 1;
    }
    if (scancode == 0x1d) { // Ctrl按下
        ctrl = 1;
    }
    if (scancode == 0x3a) { // Caps Lock按下
        caps_lock = caps_lock ^ 1;
    }
    if (scancode == 0xaa || scancode == 0xb6) { // Shift松开
        shift = 0;
    }
    if (scancode == 0x9d) { // Ctrl按下
        ctrl = 0;
    }

    printk("Keyboard scancode: %x\n", scancode);
    send_eoi();
}



void keyboard_setup(){
    register_interrupt_handler(keyboard, (void *) keyboard_handler, 0, 0x8E);
    kinfo("Setup PS/2 keyboard.");
}
