#include "speaker.h"
#include "io.h"
#include "timer.h"

static void play_sound(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t  tmp;

    Div = 1193180 / nFrequence;
    io_out8(0x43, 0xb6);
    io_out8(0x42, (uint8_t)(Div));
    io_out8(0x42, (uint8_t)(Div >> 8));

    tmp = io_in8(0x61);
    if (tmp != (tmp | 3)) { io_out8(0x61, tmp | 3); }
}

static void nosound() {
    uint8_t tmp = io_in8(0x61) & 0xFC;
    io_out8(0x61, tmp);
}

void beepX(uint32_t n, uint32_t d) {
    play_sound(n);
    nsleep(d * 1000);
    nosound();
}

void beep() {
    play_sound(1175);
    nsleep(10 * 1000);
    nosound();
}
