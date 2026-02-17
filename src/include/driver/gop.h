#pragma once

#include "boot.h"
#include "types.h"

void gop_clear(struct boot_framebuffer *framebuffer, uint32_t color);
void init_gop();
