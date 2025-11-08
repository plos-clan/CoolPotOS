#include "krlibc.h"
#include "mem/heap.h"
#include "term/klog.h"

#define WT         size_t
#define WS         (sizeof(WT))
#define SS         (sizeof(size_t))
#define ALIGN      (sizeof(size_t) - 1)
#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)
#define BITOP(a, b, op)                                                                            \
    ((a)[(size_t)(b) / (8 * sizeof *(a))] op(size_t) 1 << ((size_t)(b) % (8 * sizeof *(a))))

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

int64_t strtol(const char *str, char **endptr,
               int base) { // NOLINT(*-function-cognitive-complexity)
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
    if ((void *)endptr != NULL) *endptr = (char *)(any ? s - 1 : str);
    return (int64_t)(acc);
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

void not_null_assert(void *ptr, const char *msg) {
    if (unlikely(ptr == NULL)) {
        arch_close_interrupt();
        kerror("NullPointerError: %s", msg);
        arch_wait_for_interrupt();
    }
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

char *strrchr(const char *s, int c) {
    char *last = NULL;
    while (*s) {
        if (*s == (char)c) { last = (char *)s; }
        s++;
    }
    return (c == '\0') ? (char *)s : last;
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

static char *twobyte_strstr(const unsigned char *h, const unsigned char *n) {
    uint16_t nw = n[0] << 8 | n[1], hw = h[0] << 8 | h[1];
    for (h++; *h && hw != nw; hw = hw << 8 | *++h)
        ;
    return *h ? (char *)h - 1 : 0;
}

static char *threebyte_strstr(const unsigned char *h, const unsigned char *n) {
    uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8;
    uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8;
    for (h += 2; *h && hw != nw; hw = (hw | *++h) << 8)
        ;
    return *h ? (char *)h - 2 : 0;
}

static char *fourbyte_strstr(const unsigned char *h, const unsigned char *n) {
    uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8 | n[3];
    uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];
    for (h += 3; *h && hw != nw; hw = hw << 8 | *++h)
        ;
    return *h ? (char *)h - 3 : 0;
}

static char *twoway_strstr(const unsigned char *h, const unsigned char *n) {
    const unsigned char *z;
    size_t               l, ip, jp, k, p, ms, p0, mem, mem0;
    size_t               byteset[32 / sizeof(size_t)] = {0};
    size_t               shift[256];

    /* Computing length of needle and fill shift table */
    for (l = 0; n[l] && h[l]; l++)
        BITOP(byteset, n[l], |=), shift[n[l]] = l + 1;
    if (n[l]) return 0; /* hit the end of h */

    /* Compute maximal suffix */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k   = 1;
            } else
                k++;
        } else if (n[ip + k] > n[jp + k]) {
            jp += k;
            k   = 1;
            p   = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    ms = ip;
    p0 = p;

    /* And with the opposite comparison */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k   = 1;
            } else
                k++;
        } else if (n[ip + k] < n[jp + k]) {
            jp += k;
            k   = 1;
            p   = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    if (ip + 1 > ms + 1)
        ms = ip;
    else
        p = p0;

    /* Periodic needle? */
    if (memcmp(n, n + p, ms + 1)) {
        mem0 = 0;
        p    = MAX(ms, l - ms - 1) + 1;
    } else
        mem0 = l - p;
    mem = 0;

    /* Initialize incremental end-of-haystack pointer */
    z = h;

    /* Search loop */
    for (;;) {
        /* Update incremental end-of-haystack pointer */
        if (z - h < l) {
            /* Fast estimate for MIN(l,63) */
            size_t               grow = l | 63;
            const unsigned char *z2   = memchr(z, 0, grow);
            if (z2) {
                z = z2;
                if (z - h < l) return 0;
            } else
                z += grow;
        }

        /* Check last byte first; advance by shift on mismatch */
        if (BITOP(byteset, h[l - 1], &)) {
            k = l - shift[h[l - 1]];
            //printf("adv by %zu (on %c) at [%s] (%zu;l=%zu)\n", k, h[l-1], h, shift[h[l-1]], l);
            if (k) {
                if (mem0 && mem && k < p) k = l - p;
                h   += k;
                mem  = 0;
                continue;
            }
        } else {
            h   += l;
            mem  = 0;
            continue;
        }

        /* Compare right half */
        for (k = MAX(ms + 1, mem); n[k] && n[k] == h[k]; k++)
            ;
        if (n[k]) {
            h   += k - ms;
            mem  = 0;
            continue;
        }
        /* Compare left half */
        for (k = ms + 1; k > mem && n[k - 1] == h[k - 1]; k--)
            ;
        if (k <= mem) return (char *)h;
        h   += p;
        mem  = mem0;
    }
}

char *strstr(const char *h, const char *n) {
    /* Return immediately on empty needle */
    if (!n[0]) return (char *)h;

    /* Use faster algorithms for short needles */
    h = strchr(h, *n);
    if (!h || !n[1]) return (char *)h;
    if (!h[1]) return 0;
    if (!n[2]) return twobyte_strstr((void *)h, (void *)n);
    if (!h[2]) return 0;
    if (!n[3]) return threebyte_strstr((void *)h, (void *)n);
    if (!h[3]) return 0;
    if (!n[4]) return fourbyte_strstr((void *)h, (void *)n);

    return twoway_strstr((void *)h, (void *)n);
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
