#include "krlibc.h"
#include "heap.h"
#include "sprintf.h"

#define WT         size_t
#define WS         (sizeof(WT))
#define SS         (sizeof(size_t))
#define ALIGN      (sizeof(size_t) - 1)
#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

void close_interrupt_native() {
    __asm__ volatile("cli" ::: "memory");
}

void open_interrupt_native() {
    __asm__ volatile("sti" ::: "memory");
}

int memcmp(const void *a_, const void *b_, size_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

__attribute__((naked)) void *memcpy(void *dest, const void *src, size_t n) {
    __asm__ volatile("mov   %rdi, %rax\n\t" // 返回值 = dest
                     "mov   %eax, %r8d\n\t"
                     "neg   %r8d\n\t"
                     "and   $0x7, %r8d\n\t" // 对齐到8字节边界
                     "cmp   %r8, %rdx\n\t"
                     "cmovb %rdx, %r8\n\t" // 如果 n < 对齐字节数，就只复制 n
                     "mov   %r8, %rcx\n\t"
                     "rep movsb\n\t" // 复制对齐前的字节
                     "sub   %r8, %rdx\n\t"
                     "mov   %rdx, %rcx\n\t"
                     "shr   $0x3, %rcx\n\t"
                     "rep movsq\n\t" // 按8字节复制
                     "and   $0x7, %rdx\n\t"
                     "mov   %rdx, %rcx\n\t"
                     "rep movsb\n\t" // 复制剩余字节
                     "ret\n\t");
}

// musllibc 高速优化 memset 实现
void *memset(void *dest, int c, size_t n) {
    unsigned char *s = dest;
    size_t         k;

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

void *memmove(void *dest, const void *src, size_t n) {
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
        const size_t *w;
        size_t        k = ONES * c;
        for (w = (const void *)s; n >= SS && !HASZERO(*w ^ k); w++, n -= SS)
            ;
        for (s = (const void *)w; n && *s != c; s++, n--)
            ;
    }
    return n ? (void *)s : 0;
}

size_t strnlen(const char *str, size_t maxlen) {
    if (str == NULL) { return 0; }

    const char *p = str;
    while (maxlen-- > 0 && *p) {
        p++;
    }

    return (size_t)(p - str);
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
    const unsigned char *p1 = (const unsigned char *)s1, *p2 = (const unsigned char *)s2;
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
        if (*s1 != '\0') {
            return 1;
        } else if (*s2 != '\0') {
            return -1;
        } else {
            return 0;
        }
    } else {
        return (int)(*s1 - *s2);
    }
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

int64_t strtol(const char *str, char **endptr, int base) {
    const char *s;
    uint64_t    acc;
    char        c;
    uint64_t    cutoff;
    uint64_t    neg, any, cutlim;
    s = str;
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
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (!any) {
    noconv: {}
    } else if (neg)
        acc = -acc;
    if (endptr != NULL) *endptr = (char *)(any ? s - 1 : str);
    return (acc);
}

char *strdup(const char *str) {
    if (str == NULL) return NULL;

    char *strat = (char *)str;
    int   len   = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *)malloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {}

    return ret - (len + 1);
}

char *strndup(const char *s, size_t n) {
    if (s == NULL) { return NULL; }

    size_t actual_len = strlen(s);
    if (n < actual_len) { actual_len = n; }
    char *new_str = (char *)malloc(actual_len + 1);
    if (new_str == NULL) { return NULL; }

    size_t i;
    for (i = 0; i < actual_len; i++) {
        new_str[i] = s[i];
    }

    new_str[actual_len] = '\0';

    return new_str;
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
        if (*s == (char)c) { last = (char *)s; }
        s++;
    }
    return (c == '\0') ? (char *)s : last;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int atoi(const char *pstr) {
    int Ret_Integer  = 0;
    int Integer_sign = 1;

    if (pstr == NULL) { return 0; }
    while (isspace(*pstr) == 0) {
        pstr++;
    }
    if (*pstr == '-') { Integer_sign = -1; }
    if (*pstr == '-' || *pstr == '+') { pstr++; }
    while (*pstr >= '0' && *pstr <= '9') {
        Ret_Integer = Ret_Integer * 10 + *pstr - '0';
        pstr++;
    }
    Ret_Integer = Integer_sign * Ret_Integer;

    return Ret_Integer;
}

int fls(unsigned int x) {
    if (x == 0) return 0;
    return 32 - __builtin_clz(x);
}

char *pathacat(char *p1, char *p2) {
    char *p = (char *)malloc(strlen(p1) + strlen(p2) + 2);
    if (p1[strlen(p1) - 1] == '/') {
        sprintf(p, "%s%s", p1, p2);
    } else {
        sprintf(p, "%s/%s", p1, p2);
    }
    return p;
}

int cmd_parse(const char *cmd_str, char **argv, char token) {
    int         argc = 0;
    const char *next = cmd_str;

    while (*next) {
        while (*next == token)
            next++;
        if (*next == '\0') break;
        const char *start = next;
        while (*next && *next != token)
            next++;
        size_t len = next - start;
        argv[argc] = (char *)malloc(len + 1);
        if (!argv[argc]) {
            for (int i = 0; i < argc; i++)
                free(argv[i]);
            return -1;
        }
        memcpy(argv[argc], start, len);
        argv[argc][len] = '\0';

        argc++;
        if (argc >= 50) break;
    }
    argv[argc] = NULL;
    return argc;
}

void cmd_free(char **argv, int argc) {
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
}

char *normalize_path(const char *path) {
    if (!path) return NULL;

    size_t len    = strlen(path);
    char  *result = malloc(len + 1);
    if (!result) return NULL;

    char *dup = strdup(path);
    if (!dup) {
        free(result);
        return NULL;
    }

    strcpy(result, "/");
    if (strcmp(path, "/") == 0) {
        free(dup);
        return result;
    }

    char *start = dup;
    if (*start == '/') start++;

    char *token = strtok(start, "/");
    while (token) {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            char *last_slash = strrchr(result, '/');
            if (last_slash != result)
                *last_slash = '\0';
            else
                result[1] = '\0';
        } else {
            if (result[strlen(result) - 1] != '/') strcat(result, "/");
            strcat(result, token);
        }

        token = strtok(NULL, "/");
    }

    free(dup);
    return result;
}

size_t envp_length(char **envp) {
    size_t count = 0;
    while (envp[count] != NULL) {
        count++;
    }
    return count;
}

char **copy_envp(char **envp) {
    size_t count = 0;
    while (envp[count] != NULL) {
        count++;
    }
    char **new_envp = malloc((count + 1) * sizeof(char *));
    if (!new_envp) return NULL;
    for (size_t i = 0; i < count; i++) {
        new_envp[i] = strdup(envp[i]);
        if (!new_envp[i]) {
            for (size_t j = 0; j < i; j++) {
                free(new_envp[j]);
            }
            free(new_envp);
            return NULL;
        }
    }
    new_envp[count] = NULL;
    return new_envp;
}

void free_envp(char **envp) {
    if (!envp) return;
    for (size_t i = 0; envp[i] != NULL; i++) {
        free(envp[i]);
    }
    free(envp);
}

char *get_parent_path(const char *path) {
    if (!path || !*path) return strdup(".");

    char *copy = strdup(path);
    if (!copy) return NULL;

    char *last_slash = strrchr(copy, '/');

    if (last_slash && last_slash != copy) {
        *last_slash = '\0';
    } else if (last_slash == copy) {
        copy[1] = '\0';
    } else {
        free(copy);
        return strdup(".");
    }

    return copy;
}
