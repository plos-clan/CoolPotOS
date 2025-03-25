#pragma once

/**
 * 定义cpinl内核的各种属性
 */
#define KERNEL_NAME    "cpinl-x86_64-0.1.2"     // 内核编号
#define MAX_CPU        (256)                    // 最大支持CPU核心数 256
#define STACK_SIZE     32768                    // 栈大小(byte)
#define KERNEL_ST_SZ   131072                   // 内核栈大小 128k
#define MAX_WAIT_INDEX 100000                   // 阻塞最大循环数

// 常用工具宏
#define cpu_hlt infinite_loop __asm__("hlt")
#define UNUSED(expr)                                                                               \
    do {                                                                                           \
        (void)(expr);                                                                              \
    } while (0)
#define __IRQHANDLER __attribute__((interrupt))

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)

#define waitif(cond)                                                                               \
    ((void)({                                                                                      \
        while (cond) {}                                                                            \
    }))

#define streq(s1, s2)                                                                              \
    ({                                                                                             \
        const char *_s1 = (s1), *_s2 = (s2);                                                       \
        (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                         \
    })

#include "ctype.h"
#include "limits.h"

static inline void empty() {}

int memcmp(const void *a_, const void *b_, size_t size);

void *memcpy(void *s, const void *ct, size_t n);

void *memset(void *dst, int val, size_t size);

size_t strlen(const char *str);

char *strcat(char *dest, const char *src);

int strncmp(const char *s1, const char *s2, size_t n);

char *strcpy(char *dest, const char *src);

int strcmp(const char *s1, const char *s2);

int64_t strtol(const char *str, char **endptr, int base);

char *strdup(const char *str);

char *strchrnul(const char *s, int c);

int isspace(int c);

char *strchr(const char *s, int c);

char *strncpy(char *dest, const char *src, size_t n);

char *strrchr(const char *s, int c);

static inline char *LeadingWhitespace(char *beg, char *end) {
    while (end > beg && *--end <= 0x20) {
        *end = 0;
    }
    while (beg < end && *beg <= 0x20) {
        beg++;
    }
    return beg;
}
