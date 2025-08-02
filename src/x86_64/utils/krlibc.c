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
    char       *d = (char *)dest;
    const char *s = (const char *)src;

    void *ret = dest;

    if (n < 8) {
        while (n--) {
            *d++ = *s++;
        }
        return ret;
    }

    size_t align = (size_t)d & (sizeof(size_t) - 1);
    if (align) {
        align  = sizeof(size_t) - align;
        n     -= align;
        while (align--) {
            *d++ = *s++;
        }
    }

    size_t       *dw = (size_t *)d;
    const size_t *sw = (const size_t *)s;
    for (size_t i = 0; i < n / sizeof(size_t); i++) {
        *dw++ = *sw++;
    }

    d             = (char *)dw;
    s             = (const char *)sw;
    size_t remain = n & (sizeof(size_t) - 1);
    while (remain--) {
        *d++ = *s++;
    }

    return ret;
}

void *memset(void *dst, int val, size_t size) {
    unsigned char *d = dst;
    unsigned char  v = (unsigned char)val;

    while (size && ((size_t)d & 7)) {
        *d++ = v;
        size--;
    }

    size_t v8 = v * 0x0101010101010101ULL;
    while (size >= 8) {
        *(size_t *)d  = v8;
        d            += 8;
        size         -= 8;
    }

    while (size--)
        *d++ = v;

    return dst;
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
    int arg_idx = 0;

    while (arg_idx < 50) {
        argv[arg_idx] = 0;
        arg_idx++;
    }
    char *next = (char *)cmd_str;
    int   argc = 0;

    while (*next) {
        while (*next == token)
            next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token)
            next++;
        if (*next) { *next++ = 0; }
        if (argc > 50) return -1;
        argc++;
    }
    return argc;
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
