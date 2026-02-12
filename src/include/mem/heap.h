#pragma once

#define ALIGNED_BASE 0x1000

#if defined(__x86_64__) || defined(__amd64__)

#include "alloc.h"

#else

#include "alloc/alloc.h"

#endif


void init_heap();
