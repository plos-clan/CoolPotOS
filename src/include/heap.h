#pragma once

#define ALIGNED_BASE 0x1000

#include "limine.h"
#include "alloc.h"
#include "serial.h"

uint64_t get_all_memusage();
void init_heap();
