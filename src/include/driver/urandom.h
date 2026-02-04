#pragma once

#include "types.h"

void urandom_init();

bool arch_get_random_bytes(uint8_t *buf, size_t size);
