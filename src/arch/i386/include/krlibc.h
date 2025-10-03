#pragma once

#define KERNEL_NAME "CP_Kernel-i386-0.4.4"

#define KHEAP_INITIAL_SIZE 0xf00000
#define STACK_SIZE         32768

#define USER_STACK_TOP  0xc2000000
#define USER_AREA_START 0x90000000
#define USER_AREA_SIZE  0x2000000

#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX - 1)

#define LONG_MAX 9223372036854775807L
#define LONG_MIN (-9223372036854775808L)

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)

#define TOLOWER(x) ((x) | 0x20)
#define isxdigit(c)                                                                                \
    (('0' <= (c) && (c) <= '9') || ('a' <= (c) && (c) <= 'f') || ('A' <= (c) && (c) <= 'F'))

#define mem_geti(addr)   ({ *(volatile ssize_t *)(addr); })
#define mem_geti8(addr)  ({ *(volatile int8_t *)(addr); })
#define mem_geti16(addr) ({ *(volatile int16_t *)(addr); })
#define mem_geti32(addr) ({ *(volatile int32_t *)(addr); })
#define mem_geti64(addr) ({ *(volatile int64_t *)(addr); })

#define mem_getu(addr)   ({ *(volatile size_t *)(addr); })
#define mem_getu8(addr)  ({ *(volatile uint8_t *)(addr); })
#define mem_getu16(addr) ({ *(volatile uint16_t *)(addr); })
#define mem_getu32(addr) ({ *(volatile uint32_t *)(addr); })
#define mem_getu64(addr) ({ *(volatile uint64_t *)(addr); })

#define mem_get(addr)   mem_getu(addr)
#define mem_get8(addr)  mem_getu8(addr)
#define mem_get16(addr) mem_getu16(addr)
#define mem_get32(addr) mem_getu32(addr)
#define mem_get64(addr) mem_getu64(addr)

#define mem_set(addr, val)   ({ *(volatile size_t *)(addr) = (size_t)(val); })
#define mem_set8(addr, val)  ({ *(volatile uint8_t *)(addr) = (uint8_t)(val); })
#define mem_set16(addr, val) ({ *(volatile uint16_t *)(addr) = (uint16_t)(val); })
#define mem_set32(addr, val) ({ *(volatile uint32_t *)(addr) = (uint32_t)(val); })
#define mem_set64(addr, val) ({ *(volatile uint64_t *)(addr) = (uint64_t)(val); })

#define page_line(ptr)                                                                             \
    do {                                                                                           \
        alloc_frame_line(get_page((uint32_t)ptr, 1, get_current_directory()), (uint32_t)ptr, 1,    \
                         1);                                                                       \
    } while (0)

#define swap32(x)                                                                                  \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | (((x) & 0x0000ff00) << 8) |          \
     (((x) & 0x000000ff) << 24))
#define swap16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#define waitif(cond)                                                                               \
    ((void)({                                                                                      \
        while (cond) {}                                                                            \
    }))

#define assert(condition, ...) (void)0

#define MASK8(n)  ((uint8_t)1 << (n))
#define MASK16(n) ((uint16_t)1 << (n))
#define MASK32(n) ((uint32_t)1 << (n))
#define MASK64(n) ((uint64_t)1 << (n))
#define MASK(n)   ((size_t)1 << (n))

#define LITTLE_ENDIAN 1

#if LITTLE_ENDIAN

#    define MAGIC16(a, b) ((uint16_t)(a) | ((uint16_t)(b) << 8))
#    define MAGIC32(a, b, c, d)                                                                    \
        ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#    define MAGIC64(a, b, c, d, e, f, g, h)                                                        \
        (MAGIC32(a, b, c, d) | ((uint64_t)MAGIC32(e, f, g, h) << 32))

#else

#    define MAGIC16(a, b) ((uint16_t)(a) << 8 | (uint16_t)(b))
#    define MAGIC32(a, b, c, d)                                                                    \
        ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#    define MAGIC64(a, b, c, d, e, f, g, h)                                                        \
        (((uint64_t)MAGIC32(a, b, c, d) << 32) | MAGIC32(e, f, g, h))

#endif

#ifndef __cplusplus

#    define min(a, b)                                                                              \
        ({                                                                                         \
            auto _a = (a);                                                                         \
            auto _b = (b);                                                                         \
            _a < _b ? _a : _b;                                                                     \
        })

#    define max(a, b)                                                                              \
        ({                                                                                         \
            auto _a = (a);                                                                         \
            auto _b = (b);                                                                         \
            _a > _b ? _a : _b;                                                                     \
        })

#endif

#include "ctype.h"
#include <stdarg.h>

static inline void bzero(void *_s, size_t _n) {
    for (size_t i = 0; i < _n; i++) {
        ((uint8_t *)_s)[i] = 0;
    }
}

typedef struct _xstr {
    size_t len;
    size_t hash;
    char   data[];
} *xstr;

float roundf(float number);

float ceilf(float x);

float floorf(float x);

double fabs(double x);

double ceil(double x);

double floor(double x);

double fmod(double x, double y);

double cos(double x);

double acos(double x);

double sqrt(double number);

double pow(double x, int y);

int atoi(const char *string);

int vsprintf(char *buf, const char *fmt, va_list args);

int sprintf(char *buf, const char *fmt, ...);

void memclean(char *s, int len);

void *memcpy(void *s, const void *ct, size_t n);

int memcmp(const void *a_, const void *b_, uint32_t size);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t num);

void *memchr(const void *_s, int _c, size_t _n);

size_t strnlen(const char *s, size_t maxlen);

size_t strlen(const char *str);

char *strchr(const char *str, int c);

int64_t strtol(const char *str, char **endptr, int base);

char *strstr(char *str1, char *str2);

char *strncpy(char *dest, const char *src, unsigned long long count);

char *strdup(const char *str);

void strtoupper(char *str);

char *strchrnul(const char *s, int c);

int strcmp(const char *s1, const char *s2);

int strncmp(const char *str1, const char *str2, size_t n);

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

int isspace(int c);

int isdigit(int c);

int isalpha(int c);

int isupper(int c);

void explicit_bzero(void *_s, size_t _n);

void sleep(uint32_t time);

void shutdown(); // main.c
void reboot();   // main.c

#define streq(s1, s2)                                                                              \
    ({                                                                                             \
        char *_s1 = (s1), *_s2 = (s2);                                                             \
        (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                         \
    })

#define strneq(s1, s2, n)                                                                          \
    ({                                                                                             \
        char *_s1 = (s1), *_s2 = (s2);                                                             \
        (_s1 && _s2) ? strncmp(_s1, _s2, n) == 0 : _s1 == _s2;                                     \
    })

#define xstreq(s1, s2)                                                                             \
    ({                                                                                             \
        xstr _s1 = (s1), *_s2 = (s2);                                                              \
        (_s1 && _s2) ? (_s1->hash == _s2->hash ? xstrcmp(_s1, _s2) == 0 : false) : _s1 == _s2;     \
    })

#define memeq(s1, s2, n)                                                                           \
    ({                                                                                             \
        char *_s1 = (s1), *_s2 = (s2);                                                             \
        (_s1 && _s2) ? memcmp(_s1, _s2, n) == 0 : _s1 == _s2;                                      \
    })