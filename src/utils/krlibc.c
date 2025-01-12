#include "krlibc.h"

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

void* memset(void* dst, int val, size_t size) {
    unsigned char* d = dst;
    unsigned char v = (unsigned char)val;

    while(size && ((size_t)d & 7)) {
        *d++ = v;
        size--;
    }

    size_t v8 = v * 0x0101010101010101ULL;
    while(size >= 8) {
        *(size_t*)d = v8;
        d += 8;
        size -= 8;
    }

    while(size--) *d++ = v;

    return dst;
}

size_t strlen(const char *str) {
    const char *s = str;
    while (*s) {
        s++;
    }
    return s - str;
}

char* strcat(char* dest, const char* src) {
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}
