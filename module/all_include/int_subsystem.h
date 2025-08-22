#pragma once

#include "cp_kernel.h"

void     register_interrupt_handler(uint16_t vector, void *handler, uint8_t ist, uint8_t flags);
void     send_eoi();
uint64_t nano_time();
size_t   sched_clock();
