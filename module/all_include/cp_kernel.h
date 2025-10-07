#pragma once

#include "types/stdbool.h"
#include "types/stddef.h"
#include "types/stdint.h"

typedef __INTPTR_TYPE__ ssize_t;
typedef __INTPTR_TYPE__ ptrdiff_t;
typedef size_t          usize;
typedef ssize_t         isize;
typedef int             wchar_t;

#define ABS(x)    ((x) > 0 ? (x) : -(x))
#define MAX(x, y) ((x > y) ? (x) : (y))
#define MIN(x, y) ((x < y) ? (x) : (y))

typedef int pid_t;
typedef int errno_t;

void logkf(char *fmt, ...);
void printk(const char *fmt, ...);

void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t newsize);
void *calloc(size_t n, size_t size);

int    memcmp(const void *a_, const void *b_, size_t size);
void  *memcpy(void *dest, const void *src, size_t n);
void  *memset(void *dest, int val, size_t size);
size_t strnlen(const char *str, size_t maxlen);
size_t strlen(const char *str);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
int    sprintf(char *buf, char const *fmt, ...);
int    snprintf(char *buf, int count, const char *fmt, ...);

static inline void arch_enable_interrupt() {
    __asm__ volatile("sti");
}

static inline void arch_disable_interrupt() {
    __asm__ volatile("cli");
}
