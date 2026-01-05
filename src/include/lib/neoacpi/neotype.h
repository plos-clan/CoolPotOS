#pragma once

#ifndef NO_STD_TYPE_DEF
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdarg.h"
#include "limits.h"
#else

typedef __SIZE_TYPE__    size_t;

#ifndef __cplusplus
#    define NULL ((void *)0)
#else
#    define NULL 0

#endif

typedef __builtin_va_list va_list;

#undef va_start
#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
#    define va_start(v, ...) __builtin_va_start(v, 0)
#else
#    define va_start(v, l) __builtin_va_start(v, l)
#endif
#undef va_end
#define va_end(v) __builtin_va_end(v)
#undef va_arg
#define va_arg(v, l) __builtin_va_arg(v, l)
#if (defined(__cplusplus) && (__cplusplus >= 201103L)) ||                                          \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))
#    undef va_copy
#    define va_copy(d, s) __builtin_va_copy(d, s)
#endif

#undef UCHAR_MAX
#if __SCHAR_MAX__ == __INT_MAX__
#    define UCHAR_MAX (SCHAR_MAX * 2U + 1U)
#else
#    define UCHAR_MAX (SCHAR_MAX * 2 + 1)
#endif

#endif // NO_STD_TYPE_DEF

#define neo_acpi_unlikely(expr) __builtin_expect(!!(expr), 0)
#define neo_acpi_likely(expr) __builtin_expect(!!(expr), 1)

typedef uint64_t neo_acpi_phys_addr;

typedef va_list neo_acpi_va_list;
#define neo_acpi_va_start va_start
#define neo_acpi_va_end va_end
#define neo_acpi_va_arg va_arg
