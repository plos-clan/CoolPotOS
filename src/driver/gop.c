#include "driver/gop.h"
#include "driver/tty.h"
#include "lib/sprintf.h"
#include "limine.h"
#include "mem/heap.h"

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

void init_gop() {
    struct limine_framebuffer_response *response = get_framebuffer_response();
    for (size_t i = 0; i < response->framebuffer_count; i++) {
        struct limine_framebuffer *framebuffer = response->framebuffers[i];

        tty_device_t         *device   = alloc_tty_device(TTY_DEVICE_GRAPHI);
        struct tty_graphics_ *graphics = malloc(sizeof(struct tty_graphics_));

        graphics->address = framebuffer->address;
        graphics->width   = framebuffer->width;
        graphics->height  = framebuffer->height;
        graphics->bpp     = framebuffer->bpp;
        graphics->pitch   = framebuffer->pitch;

        graphics->blue_mask_shift  = framebuffer->blue_mask_shift;
        graphics->red_mask_shift   = framebuffer->red_mask_shift;
        graphics->green_mask_shift = framebuffer->green_mask_shift;
        graphics->blue_mask_size   = framebuffer->blue_mask_size;
        graphics->red_mask_size    = framebuffer->red_mask_size;
        graphics->green_mask_size  = framebuffer->green_mask_size;

        device->private_data = graphics;

        char name[32];
        sprintf(name, "tty%zu", i);
        strcpy(device->name, name);
        register_tty_device(device);
    }
}
