#ifndef CRASHPOWEROS_IMAGE_H
#define CRASHPOWEROS_IMAGE_H

#include "../include/stdio.h"

typedef struct {
    uint8_t r, g, b, a;
} rgb_t;

void convert_ABGR_to_ARGB(uint32_t *bitmap, size_t num_pixels);

void draw_image(char *filename);

void draw_image_xy(char *filename, uint32_t x, uint32_t y);

void draw_image_xys(char *filename, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

#endif
