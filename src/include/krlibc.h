#pragma once

#define KERNEL_NAME "CP_Kernel-x86_64-0.0.5"

#define STACK_SIZE 32768

#define cpu_hlt while(1) __asm__("hlt")
#define UNUSED(expr) do { (void)(expr); } while (0)
#define __IRQHANDLER __attribute__((interrupt))

#define waitif(cond)                                                                               \
  ((void)({                                                                                        \
    while (cond) {}                                                                                \
  }))

#define streq(s1, s2)                                                                              \
  ({                                                                                               \
    const char* _s1 = (s1), *_s2 = (s2);                                                                 \
    (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                             \
  })

#include "limits.h"
#include "ctype.h"

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

int isspace(int c);
