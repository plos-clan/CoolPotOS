#pragma once

#include "ctypes.h"

void* load_font(const char* path);
void ttf_putchar(uint32_t *vram,void* info0,char c,int cx,int cy,int *width,int *height,int size);
void ttf_print(uint32_t *vram,void *info0,char* data,int x,int y,int size);