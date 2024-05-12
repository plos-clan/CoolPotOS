#include "../include/memory.h"

void memclean(char *s, int len) {
    // 清理某个内存区域（全部置0）
    int i;
    for (i = 0; i != len; i++) {
        s[i] = 0;
    }
    return;
}

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

void *memmove(void *dest, const void *src, size_t num) {
    void *ret = dest;
    if (dest < src) {
        while (num--)//前->后
        {
            *(char *) dest = *(char *) src;
            dest = (char *) dest + 1;
            src = (char *) src + 1;
        }
    } else {
        while (num--)//后->前
        {
            *((char *) dest + num) = *((char *) src + num);
        }
    }
    return ret;
}


void *realloc(void *ptr, uint32_t size) {
    void *new = kmalloc(size);
    if (ptr) {
        memcpy(new, ptr, *(int *)((int)ptr - 4));
        kfree(ptr);
    }
    return new;
}