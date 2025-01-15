#pragma once

#include "ctype.h"

enum InterruptIndex {
    timer = 32,
    keyboard,
    mouse,
    hpet_timer,
};

struct interrupt_frame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, rip, rflags;
    uint64_t cs, ss, ds, es, fs, gs;
};

typedef struct interrupt_frame interrupt_frame_t;
void kernel_error(const char *msg,uint64_t code,interrupt_frame_t *frame);

void register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags);
void send_eoi();
