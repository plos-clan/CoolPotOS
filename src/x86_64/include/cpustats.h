#pragma once

#include "ctype.h"

uint64_t read_tsc();
size_t   sched_clock();
void     calibrate_tsc_with_hpet();
