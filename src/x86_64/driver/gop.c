#include "gop.h"
#include "boot.h"
#include "ctype.h"
#include "krlibc.h"
#include <stddef.h>

struct limine_framebuffer *framebuffer = NULL;

uint64_t get_screen_width() {
    return framebuffer->width;
}

uint64_t get_screen_height() {
    return framebuffer->height;
}

void init_gop() {
    framebuffer = get_graphics_framebuffer();
    if (framebuffer == NULL) cpu_hlt;
    gop_clear(0xffffff);
}

void gop_clear(uint32_t color) {
    uint64_t stride = framebuffer->pitch / 4;

    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            *((uint32_t *)framebuffer->address + (y * stride + x)) = color;
        }
    }
}
