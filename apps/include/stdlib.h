#ifndef CRASHPOWEROS_STDLIB_H
#define CRASHPOWEROS_STDLIB_H

#include "ctype.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

long long atoi(const char* s);
void *malloc(size_t size);
void free(void *ptr);
void exit(int code);
void *realloc(void *ptr, uint32_t size);
void *calloc(size_t n, size_t size);
void abort();
void goto_xy(short x, short y);
int system(char *command);

#endif
