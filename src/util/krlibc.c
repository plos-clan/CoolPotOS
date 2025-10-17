#include "krlibc.h"

#define WT         size_t
#define WS         (sizeof(WT))
#define SS         (sizeof(size_t))
#define ALIGN      (sizeof(size_t) - 1)
#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

int memcmp(const void *a_, const void *b_, size_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memset(void *dest, int c, size_t n) {
    unsigned char *s = dest;
    size_t         k = 0;

    if (!n) return dest;
    s[0]     = c;
    s[n - 1] = c;
    if (n <= 2) return dest;
    s[1]     = c;
    s[2]     = c;
    s[n - 2] = c;
    s[n - 3] = c;
    if (n <= 6) return dest;
    s[3]     = c;
    s[n - 4] = c;
    if (n <= 8) return dest;

    k  = -(uintptr_t)s & 3;
    s += k;
    n -= k;
    n &= -4;

#ifdef __GNUC__
    typedef uint32_t __attribute__((__may_alias__)) u32;
    typedef uint64_t __attribute__((__may_alias__)) u64;

    u32 c32 = ((u32)-1) / 255 * (unsigned char)c;

    *(u32 *)(s + 0)     = c32;
    *(u32 *)(s + n - 4) = c32;
    if (n <= 8) return dest;
    *(u32 *)(s + 4)      = c32;
    *(u32 *)(s + 8)      = c32;
    *(u32 *)(s + n - 12) = c32;
    *(u32 *)(s + n - 8)  = c32;
    if (n <= 24) return dest;
    *(u32 *)(s + 12)     = c32;
    *(u32 *)(s + 16)     = c32;
    *(u32 *)(s + 20)     = c32;
    *(u32 *)(s + 24)     = c32;
    *(u32 *)(s + n - 28) = c32;
    *(u32 *)(s + n - 24) = c32;
    *(u32 *)(s + n - 20) = c32;
    *(u32 *)(s + n - 16) = c32;

    k  = 24 + ((uintptr_t)s & 4);
    s += k;
    n -= k;

    u64 c64 = c32 | ((u64)c32 << 32);
    for (; n >= 32; n -= 32, s += 32) {
        *(u64 *)(s + 0)  = c64;
        *(u64 *)(s + 8)  = c64;
        *(u64 *)(s + 16) = c64;
        *(u64 *)(s + 24) = c64;
    }
#else
    /* Pure C fallback with no aliasing violations. */
    for (; n; n--, s++)
        *s = c;
#endif

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) { // NOLINT(*-function-cognitive-complexity)
    char       *d = dest;
    const char *s = src;

    if (d == s) return d;
    if (s + n <= d || d + n <= s) return memcpy(d, s, n);

    if (d < s) {
        if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
            while ((uintptr_t)d % WS) {
                if (!n--) return dest;
                *d++ = *s++;
            }
            for (; n >= WS; n -= WS, d += WS, s += WS)
                *(WT *)d = *(WT *)s;
        }
        for (; n; n--)
            *d++ = *s++;
    } else {
        if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
            while ((uintptr_t)(d + n) % WS) {
                if (!n--) return dest;
                d[n] = s[n];
            }
            while (n >= WS)
                n -= WS, *(WT *)(d + n) = *(WT *)(s + n);
        }
        while (n)
            n--, d[n] = s[n];
    }

    return dest;
}

void *memchr(const void *src, int c, size_t n) {
    const unsigned char *s = src;
    c                      = (unsigned char)c;
    for (; ((uintptr_t)s & ALIGN) && n && *s != c; s++, n--)
        ;
    if (n && *s != c) {
        size_t *w = 0;
        size_t  k = ONES * c;
        for (w = (void *)s; n >= SS && !HASZERO(*w ^ k); w++, n -= SS)
            ;
        for (s = (const void *)w; n && *s != c; s++, n--)
            ;
    }
    return n ? (void *)s : 0;
}

size_t strnlen(const char *s, size_t n) {
    const char *p = memchr(s, 0, n);
    return p ? p - s : n;
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
    size_t     *w = NULL;
    for (; (uintptr_t)s % ALIGN; s++)
        if (!*s) return s - a;
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
        if ((*s++) == c) break;
    }
    return (char *)s;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n-- > 0) {
        if (*p1 != *p2) return *p1 - *p2;
        if (*p1 == '\0') return 0;
        p1++, p2++;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) { return (char *)s; }
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
        if (*s1 != '\0') { return 1; }
        if (*s2 != '\0') { return -1; }
        return 0;
    }
    return (int)(*s1 - *s2);
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

char *strtok(char *str, const char *delim) {
    static char *last = NULL;
    if (str) {
        last = str;
    } else if (!last) {
        return NULL;
    }

    char *start = last;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (*start == '\0') {
        last = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        last = end + 1;
    } else {
        last = NULL;
    }

    return start;
}

int64_t strtol(const char *str, char **endptr, int base) { // NOLINT(*-function-cognitive-complexity)
    const char *s      = str;
    uint64_t    acc    = 0;
    char        c      = '\0';
    uint64_t    cutoff = 0;
    uint64_t    neg    = 0;
    uint64_t    any    = 0;
    uint64_t    cutlim = 0;
    do {
        c = *s++;
    } while (isspace((unsigned char)c));
    if (c == '-') {
        neg = 1;
        c   = *s++;
    } else {
        neg = 0;
        if (c == '+') c = *s++;
    }
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') ||
         (s[1] >= 'a' && s[1] <= 'f'))) {
        c     = s[1];
        s    += 2;
        base  = 16;
    }
    if (base == 0) base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36) goto noconv;

    cutoff  = neg ? (unsigned long)-(LONG_MIN + LONG_MAX) + LONG_MAX : LONG_MAX;
    cutlim  = cutoff % base;
    cutoff /= base;
    for (;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && ((uint64_t)c) > cutlim))
            any = -1;
        else {
            any  = 1;
            acc *= base;
            acc += c;
        }
    }
    if (!any) {
    noconv: {}
    } else if (neg)
        acc = -acc;
    if ((void*)endptr != NULL) *endptr = (char *)(any ? s - 1 : str);
    return (int64_t)(acc);
}
