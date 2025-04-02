#include "krlibc.h"
#include "heap.h"

int memcmp(const void *a_, const void *b_, size_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *) dest;
    const char *s = (const char *) src;

    void *ret = dest;

    if (n < 8) {
        while (n--) {
            *d++ = *s++;
        }
        return ret;
    }

    size_t align = (size_t) d & (sizeof(size_t) - 1);
    if (align) {
        align = sizeof(size_t) - align;
        n -= align;
        while (align--) {
            *d++ = *s++;
        }
    }

    size_t *dw = (size_t *) d;
    const size_t *sw = (const size_t *) s;
    for (size_t i = 0; i < n / sizeof(size_t); i++) {
        *dw++ = *sw++;
    }

    d = (char *) dw;
    s = (const char *) sw;
    size_t remain = n & (sizeof(size_t) - 1);
    while (remain--) {
        *d++ = *s++;
    }

    return ret;
}

void *memset(void *dst, int val, size_t size) {
    unsigned char *d = dst;
    unsigned char v = (unsigned char) val;

    while (size && ((size_t) d & 7)) {
        *d++ = v;
        size--;
    }

    size_t v8 = v * 0x0101010101010101ULL;
    while (size >= 8) {
        *(size_t *) d = v8;
        d += 8;
        size -= 8;
    }

    while (size--)
        *d++ = v;

    return dst;
}

size_t strlen(const char *str) {
    const char *s = str;
    while (*s) {
        s++;
    }
    return s - str;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++));
    return ret;
}

char *strchrnul(const char *s, int c) {
    while (*s) {
        if ((*s++) == c) break;
    }
    return (char *) s;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *) s1, *p2 = (const unsigned char *) s2;
    while (n-- > 0) {
        if (*p1 != *p2) return *p1 - *p2;
        if (*p1 == '\0') return 0;
        p1++, p2++;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char) c) { return (char *) s; }
        s++;
    }
    return (*s == (char) c) ? (char *) s : NULL;
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
        } else if (*s2 != '\0') {
            return -1;
        } else {
            return 0;
        }
    } else {
        return (int) (*s1 - *s2);
    }
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

int64_t strtol(const char *str, char **endptr, int base) {
    const char *s;
    uint64_t acc;
    char c;
    uint64_t cutoff;
    uint64_t neg, any, cutlim;
    s = str;
    do {
        c = *s++;
    } while (isspace((unsigned char) c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+') c = *s++;
    }
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') ||
         (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36) goto noconv;

    cutoff = neg ? (unsigned long) -(LONG_MIN + LONG_MAX) + LONG_MAX : LONG_MAX;
    cutlim = cutoff % base;
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
        if (any < 0 || acc > cutoff || (acc == cutoff && ((uint64_t) c) > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (!any) {
        noconv:
        {}
    } else if (neg)
        acc = -acc;
    if (endptr != NULL) *endptr = (char *) (any ? s - 1 : str);
    return (acc);
}

char *strdup(const char *str) {
    if (str == NULL) return NULL;

    char *strat = (char *) str;
    int len = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *) malloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {}

    return ret - (len + 1);
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strrchr(const char *s, int c) {
    char *last = NULL;
    while (*s) {
        if (*s == (char) c) { last = (char *) s; }
        s++;
    }
    return (c == '\0') ? (char *) s : last;
}
