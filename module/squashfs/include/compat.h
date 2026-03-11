/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COMPAT_H
#define COMPAT_H

#include "sqfs/predef.h"
#include "config.h"
#include "types/limits.h"
#include "cp_kernel.h"
#include "fs_subsystem.h"

#if defined(__GNUC__) && __GNUC__ >= 5
#define SZ_ADD_OV __builtin_add_overflow
#define SZ_MUL_OV __builtin_mul_overflow
#elif defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#if SIZE_MAX <= UINT_MAX
#define SZ_ADD_OV __builtin_uadd_overflow
#define SZ_MUL_OV __builtin_umul_overflow
#elif SIZE_MAX == ULONG_MAX
#define SZ_ADD_OV __builtin_uaddl_overflow
#define SZ_MUL_OV __builtin_umull_overflow
#elif SIZE_MAX == ULLONG_MAX
#define SZ_ADD_OV __builtin_uaddll_overflow
#define SZ_MUL_OV __builtin_umulll_overflow
#else
#error Cannot determine maximum value of size_t
#endif
#else
static inline int _sz_add_overflow(size_t a, size_t b, size_t *res) {
    *res = a + b;
    return (*res < a) ? 1 : 0;
}

static inline int _sz_mul_overflow(size_t a, size_t b, size_t *res) {
    *res = a * b;
    return (b > 0 && (a > SIZE_MAX / b)) ? 1 : 0;
}
#define SZ_ADD_OV _sz_add_overflow
#define SZ_MUL_OV _sz_mul_overflow
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_ATTRIB(fmt, elipsis) __attribute__((format(printf, fmt, elipsis)))
#else
#define PRINTF_ATTRIB(fmt, elipsis)
#endif

#define PRIu32 "u"
#define PRIu64 "llu"
#define PRI_U64 "%" PRIu64
#define PRI_U32 "%" PRIu32

#if SIZE_MAX <= UINT_MAX
#define PRI_SZ "%u"
#elif SIZE_MAX == ULONG_MAX
#define PRI_SZ "%lu"
#elif SIZE_MAX == ULLONG_MAX
#define PRI_SZ "%llu"
#else
#error Cannot figure out proper printf specifier for size_t
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)
#else
#define htole16(x) __builtin_bswap16(x)
#define htole32(x) __builtin_bswap32(x)
#define htole64(x) __builtin_bswap64(x)
#define le16toh(x) __builtin_bswap16(x)
#define le32toh(x) __builtin_bswap32(x)
#define le64toh(x) __builtin_bswap64(x)
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif

#ifndef major
#define major(x) ((unsigned)((((x) >> 31 >> 1) & 0xfffff000U) | (((x) >> 8) & 0x00000fffU)))
#endif
#ifndef minor
#define minor(x) ((unsigned)((((x) >> 12) & 0xffffff00U) | ((x) & 0x000000ffU)))
#endif
#ifndef makedev
#define makedev(x, y) ((((x) & 0xfffff000ULL) << 32) | (((x) & 0x00000fffULL) << 8) | (((y) & 0xffffff00ULL) << 12) | (((y) & 0x000000ffULL)))
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max_len);
#endif

#ifndef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c);
#endif

#endif /* COMPAT_H */
