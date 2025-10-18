#pragma once

#define __IRQHANDLER __attribute__((interrupt))

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)
#define PADDING_REQ(size, to)  ((size + (to) - 1) & ~((to) - 1) / (to))

#define UNUSED(...)                                                                                \
    do {                                                                                           \
        (void)(0, ##__VA_ARGS__);                                                                  \
    } while (0)

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

// 分支预测优化: x 很可能为假
#define unlikely(x) __builtin_expect(!!(x), 0)

// 分支预测优化: x 很可能为真
#define likely(x) __builtin_expect(!!(x), 1)

#include "metadata.h"
#include "types.h"
#include "types/limits.h"

void arch_pause();
void arch_wait_for_interrupt();
void arch_close_interrupt();
void arch_open_interrupt();
bool arch_check_interrupt();

void not_null_assert(void *ptr, const char *msg);

void   *memset(void *dest, int c, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
void   *memchr(const void *src, int c, size_t n);
size_t  strnlen(const char *str, size_t maxlen);
size_t  strlen(const char *s);
char   *strcat(char *dest, const char *src);
char   *strchrnul(const char *s, int c);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strchr(const char *s, int c);
char   *strcpy(char *dest, const char *src);
int     strcmp(const char *s1, const char *s2);
char   *strtok(char *str, const char *delim);
char   *strdup(const char *str);
char   *strndup(const char *s, size_t n);
char   *strrchr(const char *s, int c);
int64_t strtol(const char *str, char **endptr, int base);
int     memcmp(const void *a_, const void *b_, size_t size);
void   *memcpy(void *dest, const void *src, size_t n);
int     isdigit(int c);

int isspace(int c);

int sprintf(char *buf, char const *fmt, ...);
int snprintf(char *buf, int count, const char *fmt, ...);
