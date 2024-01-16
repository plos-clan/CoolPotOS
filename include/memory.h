#ifndef CPOS_MEMORY_H
#define CPOS_MEMORY_H

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dst_, const void *src_, uint32_t size);
int memcmp(const void *a_, const void *b_, uint32_t size);
void *memset(void *s, int c, size_t n);

void install_page();

#endif