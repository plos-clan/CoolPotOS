#pragma once

#define TIMER_FREQ 10000000 /* CLINT时钟频率（通常是10MHz） */

#include "ptrace.h"
#include "types.h"

extern uint64_t timer_freq;

void     timer_init_hart(uint32_t hart_id);
void     riscv64_timer_handler(struct pt_regs *regs);
void     sbi_set_timer(uint64_t stime_value);
uint64_t get_timer(void);
