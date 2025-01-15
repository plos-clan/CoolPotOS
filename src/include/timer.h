#pragma once

#include "ctype.h"
#include "isr.h"

void hpet_clock(interrupt_frame_t *frame);
void usleep(uint64_t nano);
uint64_t nanoTime();
