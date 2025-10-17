#pragma once

#include "types.h"
#include "limine.h"

struct limine_framebuffer_response *get_framebuffer_response();
void gop_clear(struct limine_framebuffer *framebuffer, uint32_t color);
