#include "../include/memory.h"

void *memcpy(void *dst_, const void *src_, uint32_t size) {
    uint8_t *dst = dst_;
    const uint8_t *src = src_;
    while (size-- > 0) *dst++ = *src++;
    return (void *) src_;
}

int memcmp(const void *a_, const void *b_, uint32_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n-- > 0)
        *p++ = c;
    return s;
}