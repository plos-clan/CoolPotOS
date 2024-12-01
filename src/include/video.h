#pragma once

#include "ctypes.h"
#include "multiboot.h"

typedef struct video_area {
    uint32_t *screen;
    uint32_t width;
    uint32_t height;
} video_area_t;

typedef struct {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} Pixel;

void vga_move_cursor(uint16_t x, uint16_t y);
void vga_clear();
void vga_putchar(char c);
void vga_write(const char *data, size_t size);
void vga_writestring(const char *data);
void vga_install(void);

void put_bitmap(uint32_t *screen,uint8_t *bitmap, int x, int y,
                int width, int heigh, unsigned bitmap_xsize,
                unsigned xsize, unsigned fc);
void vbe_clear(uint32_t color);
void init_vbe(multiboot_t *multiboot);
uint32_t *get_vbe_screen();
uint32_t get_vbe_width();
uint32_t get_vbe_height();
