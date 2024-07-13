#include "../include/mouse.h"
#include "../include/io.h"
#include "../include/isr.h"
#include "../include/printf.h"

uint8_t mouse_cycle = 0;     //unsigned char
int8_t mouse_byte[3];    //signed char
int8_t mouse_x = 0;         //signed char
int8_t mouse_y = 0;         //signed char

static void wait_KBC_sendready(void) {
    for (;;) {
        if ((io_in8(0x0064) & 0x02) == 0) {
            break;
        }
    }
    return;
}

static void mouse_handler(registers_t *a_r) {
    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = io_in8(0x60);
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = io_in8(0x60);
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = io_in8(0x60);
            mouse_x = mouse_byte[1];
            mouse_y = mouse_byte[2];
            mouse_cycle = 0;
            break;
    }

    printf("Mouse Mov: %d %d\n",mouse_x,mouse_y);
}

void mouse_wait(uint8_t a_type) {
    unsigned int _time_out = 100000;
    if (a_type == 0) {
        while (_time_out--) {
            if ((io_in8(0x64) & 1) == 1) {
                return;
            }
        }
        return;
    } else {
        while (_time_out--) {
            if ((io_in8(0x64) & 2) == 0) {
                return;
            }
        }
        return;
    }
}

void mouse_write(uint8_t a_write) {
    mouse_wait(1);
    io_out8(0x64, 0xD4);
    mouse_wait(1);
    io_out8(0x60, a_write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return io_in8(0x60);
}

void mouse_reset() {
    mouse_write(0xff);
}

void mouse_install() {
    wait_KBC_sendready();
    io_out8(0x0064, 0xd4);
    wait_KBC_sendready();
    io_out8(0x0060, 0xf4);
    mouse_write(0xf3);
    mouse_write(200);
    mouse_write(0xf3);
    mouse_write(100);
    mouse_write(0xf3);
    mouse_write(80);
    mouse_write(0xf2);
    logkf("mouseId=%d\n", mouse_read());

    register_interrupt_handler(12,mouse_handler);
}