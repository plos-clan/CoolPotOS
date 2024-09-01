#ifndef CRASHPOWEROS_STDLIB_H
#define CRASHPOWEROS_STDLIB_H

#include <stddef.h>

long long atoi(const char* s);
void *malloc(size_t size);
void free(void *ptr);
void exit(int code);
void *realloc(void *ptr, uint32_t size);

#endif
