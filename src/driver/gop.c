#include "driver/gop.h"
#include "limine.h"

LIMINE_REQUEST struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

struct limine_framebuffer_response *get_framebuffer_response() {
    return framebuffer_request.response;
}

void gop_clear(struct limine_framebuffer *framebuffer, uint32_t color) {
    uint64_t stride = framebuffer->pitch / 4;

    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            *((uint32_t *)framebuffer->address + (y * stride + x)) = color;
        }
    }
}


