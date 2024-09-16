#ifndef CRASHPOWEROS_TTFPRINT_H
#define CRASHPOWEROS_TTFPRINT_H

#include "ctype.h"

bool ttf_install(char* filename);
uint8_t *ttf_bitmap(int *buf,uint32_t *width,uint32_t *heigh,float pixels);
void print_ttf(char* buf,uint32_t fc,uint32_t bc,uint32_t x,uint32_t y,float pixels);

#endif
