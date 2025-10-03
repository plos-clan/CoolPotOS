#pragma once

#include "cptype.h"

typedef struct {
    uint8_t fxsave_area[512] __attribute__((aligned(16)));
} fpu_context_t;

void save_fpu_context(fpu_context_t *ctx);

void restore_fpu_context(fpu_context_t *ctx);

void float_processor_setup();
