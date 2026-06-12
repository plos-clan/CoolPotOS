#include "krlibc.h"

#include "term/kprint.h"
#include "types/limits.h"

#define STB_SPRINTF_IMPLEMENTATION
#if !(defined(__x86_64__) || defined(__amd64__))
#    define STB_SPRINTF_NOFLOAT
#endif
#include "lib/sprintf.h"

int sprintf(char *buf, char const *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    const int result = STB_SPRINTF_DECORATE(vsprintfcb)(0, 0, buf, fmt, va);
    va_end(va);
    return result;
}

int snprintf(char *buf, int count, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    const int result = STB_SPRINTF_DECORATE(vsnprintf)(buf, count, fmt, va);
    va_end(va);
    return result;
}

int vsnprintf(char *buf, const int count, const char *fmt, va_list va) {
    return STB_SPRINTF_DECORATE(vsnprintf)(buf, count, fmt, va);
}

int isdigit(const int c) {
    return c >= '0' && c <= '9';
}

#ifdef ALIGN
#    undef ALIGN
#endif

#define ALIGN (sizeof(size_t))

#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

size_t strlen(const char *s) {
    const char *a = s;
    size_t *w     = NULL;
    for (; (uintptr_t)s % ALIGN; s++)
        if (!*s)
            return s - a;
    for (w = (void *)s; !HASZERO(*w); w++)
        ;
    for (s = (const void *)w; *s; s++)
        ;
    return s - a;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
    return ret;
}

char *strchrnul(const char *s, int c) {
    while (*s) {
        if ((*s++) == c)
            break;
    }
    return (char *)s;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n-- > 0) {
        if (*p1 != *p2)
            return *p1 - *p2;
        if (*p1 == '\0')
            return 0;
        p1++, p2++;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return (*s == (char)c) ? (char *)s : NULL;
}

char *strcpy(char *dest, const char *src) {
    do {
        *dest++ = *src++;
    } while (*src != 0);
    *dest = 0;
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    char is_equal = 1;

    for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++) {
        if (*s1 != *s2) {
            is_equal = 0;
            break;
        }
    }

    if (is_equal) {
        if (*s1 != '\0') {
            return 1;
        }
        if (*s2 != '\0') {
            return -1;
        }
        return 0;
    }
    return (int)(*s1 - *s2);
}

char *strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }

    char *strat = (char *)str;
    int len     = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *)malloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {
    }

    return ret - (len + 1);
}

_Noreturn void panic(const char *file, int line, const char *func, const char *cond) {
    printk("assert failed! %s\n", cond);
    printk("file: %s\nline %d\nfunc: %s\n", file, line, func);

    arch_close_interrupt();
    while (true) {
        arch_wait_for_interrupt();
    }
}
