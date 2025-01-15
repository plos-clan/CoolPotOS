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

void send_eoi();
