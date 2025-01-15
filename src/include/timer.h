#pragma once

#include "ctype.h"
#include "isr.h"

void hpet_clock(interrupt_frame_t *frame);

uint64_t nanoTime();
