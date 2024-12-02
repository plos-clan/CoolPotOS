#include "speaker.h"
#include "io.h"
#include "timer.h"

static void play_sound(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;

    Div = 1193180 / nFrequence;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t) (Div) );
    outb(0x42, (uint8_t) (Div >> 8));

    tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

static void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void beepX(uint32_t n,uint32_t d){
    play_sound(n);
    clock_sleep(d);
    nosound();
}

void beep() {
    play_sound(1175);
    clock_sleep(10);
    nosound();
}
