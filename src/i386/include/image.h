#pragma once

#include "ctypes.h"

void draw_bitmap(int x,int y,int w,int h,uint8_t *buffer);
void draw_image_xys(char* filename,uint32_t x,uint32_t y,uint32_t width,uint32_t height);
void convert_ABGR_to_ARGB(uint32_t* bitmap, size_t num_pixels);