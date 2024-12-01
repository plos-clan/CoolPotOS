#include "video.h"

uint32_t width;
uint32_t height;
uint32_t *screen;

static video_area_t videoArea;

void vbe_clear(uint32_t color) {
    width = videoArea.width;
    height = videoArea.height;
    screen = videoArea.screen;
    for (uint32_t i = 0; i < (width * (height)); i++) {
        screen[i] = color;
    }
}

uint32_t LCD_AlphaBlend(uint32_t foreground_color, uint32_t background_color,
                        uint8_t alpha) {
    uint8_t *fg = (uint8_t *) &foreground_color;
    uint8_t *bg = (uint8_t *) &background_color;

    uint32_t rb = (((uint32_t) (*fg & 0xFF) * alpha) +
                   ((uint32_t) (*bg & 0xFF) * (256 - alpha))) >>
                                                              8;
    uint32_t g = (((uint32_t) (*(fg + 1) & 0xFF) * alpha) +
                  ((uint32_t) (*(bg + 1) & 0xFF) * (256 - alpha))) >>
                                                                   8;
    uint32_t a = (((uint32_t) (*(fg + 2) & 0xFF) * alpha) +
                  ((uint32_t) (*(bg + 2) & 0xFF) * (256 - alpha))) >>
                                                                   8;

    return (rb & 0xFF) | ((g & 0xFF) << 8) | ((a & 0xFF) << 16);
}

void put_bitmap(uint32_t *screen,uint8_t *bitmap, int x, int y,
                int width, int heigh, unsigned bitmap_xsize,
                unsigned xsize, unsigned fc) {
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < heigh; j++) {
            if (bitmap[j * bitmap_xsize + i] == 0) continue;
            screen[(y + j) * xsize + (x + i)] = LCD_AlphaBlend(fc, 0, bitmap[j * bitmap_xsize + i]);
        }
    }
}

uint32_t *get_vbe_screen() { return videoArea.screen; }

uint32_t get_vbe_width() { return videoArea.width; }

uint32_t get_vbe_height() { return videoArea.height; }

void init_vbe(multiboot_t *multiboot) {
    screen = videoArea.screen = (uint32_t *) multiboot->framebuffer_addr;
    width = videoArea.width = multiboot->framebuffer_width;
    height = videoArea.height = multiboot->framebuffer_height;
    vbe_clear(0xc6c6c6);
}
