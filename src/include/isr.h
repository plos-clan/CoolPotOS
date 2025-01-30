#pragma once

#include "ctype.h"

enum InterruptIndex {
    timer = 32,
    keyboard,
    mouse,
    hpet_timer,
};

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef struct interrupt_frame interrupt_frame_t;
void kernel_error(const char *msg,uint64_t code,interrupt_frame_t *frame);
void print_register(interrupt_frame_t *frame);
void register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags);
void set_kernel_stack(uint64_t rsp);
void send_eoi();
void lapic_timer_stop();
