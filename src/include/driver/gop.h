#pragma once

#include "types.h"
#include "boot.h"

void gop_clear(const struct boot_framebuffer *framebuffer, uint32_t color);
void init_gop();
