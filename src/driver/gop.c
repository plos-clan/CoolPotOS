#include "driver/gop.h"
#include "driver/tty.h"
#include "lib/sprintf.h"
#include "mem/heap.h"

void gop_clear(struct boot_framebuffer *framebuffer, uint32_t color) {
    uint64_t stride = framebuffer->pitch / 4;

    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            *((uint32_t *)framebuffer->address + (y * stride + x)) = color;
        }
    }
}

void init_gop() {
    for (size_t i = 0; i < boot_framebuffer_count(); i++) {
        struct boot_framebuffer *framebuffer = boot_get_framebuffer(i);

        tty_device_t         *device   = alloc_tty_device(TTY_DEVICE_GRAPHI);
        struct tty_graphics_ *graphics = malloc(sizeof(struct tty_graphics_));

        graphics->address = (void *)framebuffer->address;
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
