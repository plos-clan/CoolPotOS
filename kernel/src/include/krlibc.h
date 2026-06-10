#pragma once

#include "types.h"
#include "types/stdarg.h"
#include "metadata.h"
#include "arch.h"

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)
#define PADDING_REQ(size, to)  ((size + (to) - 1) & ~((to) - 1) / (to))

// 分支预测优化: x 很可能为假
#define unlikely(x) __builtin_expect(!!(x), 0)

// 分支预测优化: x 很可能为真
#define likely(x) __builtin_expect(!!(x), 1)

#define asserts(b, msg)                                                                            \
    do {                                                                                           \
        if (unlikely(!(b))) {                                                                      \
            /*extern void logkf(char *fmt, ...);                                                   \
            logkf("assertion failed: %s\n\r" msg);  */                                             \
            for (;;)                                                                               \
                arch_wait_for_interrupt();                                                         \
        }                                                                                          \
    } while (0)

#define ABS(x)    ((x) > 0 ? (x) : -(x))
#define MAX(x, y) ((x > y) ? (x) : (y))
#define MIN(x, y) ((x < y) ? (x) : (y))

#define assert(condition)                                                                          \
    if (!(condition))                                                                              \
    panic(__FILE__, __LINE__, __func__, #condition)

#define container_of(ptr, type, member)                                                            \
    ({                                                                                             \
        uint64_t __mptr = ((uint64_t)(ptr));                                                       \
        (type *)((char *)__mptr - offsetof(type, member));                                         \
    })

#define streq(s1, s2)                                                                              \
    ({                                                                                             \
        const char *_s1 = (s1), *_s2 = (s2);                                                       \
        (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                         \
    })

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

int sprintf(char *buf, char const *fmt, ...);
int snprintf(char *buf, int count, const char *fmt, ...);
int vsnprintf(char *buf, int count, const char *fmt, va_list va);

int isdigit(int c);
size_t strlen(const char *s);
char *strcat(char *dest, const char *src);
char *strchrnul(const char *s, int c);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *aligned_alloc(size_t alignment, size_t size);

void panic(const char *file, int line, const char *func, const char *cond);
