/* Copyright (C) 2022-2025 mintsuki and contributors.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __FREESTND_C_HDRS_STDINT_H
#define __FREESTND_C_HDRS_STDINT_H 1

#ifdef __UINT8_TYPE__
typedef __UINT8_TYPE__ uint8_t;
#endif
#ifdef __UINT16_TYPE__
typedef __UINT16_TYPE__ uint16_t;
#endif
#ifdef __UINT32_TYPE__
typedef __UINT32_TYPE__ uint32_t;
#endif
#ifdef __UINT64_TYPE__
typedef __UINT64_TYPE__ uint64_t;
#endif

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

#ifdef __INT8_TYPE__
typedef __INT8_TYPE__ int8_t;
#endif
#ifdef __INT16_TYPE__
typedef __INT16_TYPE__ int16_t;
#endif
#ifdef __INT32_TYPE__
typedef __INT32_TYPE__ int32_t;
#endif
#ifdef __INT64_TYPE__
typedef __INT64_TYPE__ int64_t;
#endif

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

#ifdef __UINTPTR_TYPE__
typedef __UINTPTR_TYPE__ uintptr_t;
#endif
#ifdef __INTPTR_TYPE__
typedef __INTPTR_TYPE__ intptr_t;
#endif

typedef __UINTMAX_TYPE__ uintmax_t;
typedef __INTMAX_TYPE__ intmax_t;

/* Clang and GCC have different mechanisms for INT32_C and friends. */
#ifdef __clang__
#   ifndef __FREESTND_C_HDRS_C_JOIN
#       define __FREESTND_C_HDRS_C_EXPAND_JOIN(x, suffix) x ## suffix
#       define __FREESTND_C_HDRS_C_JOIN(x, suffix) __FREESTND_C_HDRS_C_EXPAND_JOIN(x, suffix)
#   endif

#   undef INT8_C
#   define INT8_C(x) __FREESTND_C_HDRS_C_JOIN(x, __INT8_C_SUFFIX__)
#   undef INT16_C
#   define INT16_C(x) __FREESTND_C_HDRS_C_JOIN(x, __INT16_C_SUFFIX__)
#   undef INT32_C
#   define INT32_C(x) __FREESTND_C_HDRS_C_JOIN(x, __INT32_C_SUFFIX__)
#   undef INT64_C
#   define INT64_C(x) __FREESTND_C_HDRS_C_JOIN(x, __INT64_C_SUFFIX__)

#   undef UINT8_C
#   define UINT8_C(x) __FREESTND_C_HDRS_C_JOIN(x, __UINT8_C_SUFFIX__)
#   undef UINT16_C
#   define UINT16_C(x) __FREESTND_C_HDRS_C_JOIN(x, __UINT16_C_SUFFIX__)
#   undef UINT32_C
#   define UINT32_C(x) __FREESTND_C_HDRS_C_JOIN(x, __UINT32_C_SUFFIX__)
#   undef UINT64_C
#   define UINT64_C(x) __FREESTND_C_HDRS_C_JOIN(x, __UINT64_C_SUFFIX__)

#   undef INTMAX_C
#   define INTMAX_C(x) __FREESTND_C_HDRS_C_JOIN(x, __INTMAX_C_SUFFIX__)
#   undef UINTMAX_C
#   define UINTMAX_C(x) __FREESTND_C_HDRS_C_JOIN(x, __UINTMAX_C_SUFFIX__)
#else
#   undef INT8_C
#   define INT8_C(x) __INT8_C(x)
#   undef INT16_C
#   define INT16_C(x) __INT16_C(x)
#   undef INT32_C
#   define INT32_C(x) __INT32_C(x)
#   undef INT64_C
#   define INT64_C(x) __INT64_C(x)

#   undef UINT8_C
#   define UINT8_C(x) __UINT8_C(x)
#   undef UINT16_C
#   define UINT16_C(x) __UINT16_C(x)
#   undef UINT32_C
#   define UINT32_C(x) __UINT32_C(x)
#   undef UINT64_C
#   define UINT64_C(x) __UINT64_C(x)

#   undef INTMAX_C
#   define INTMAX_C(x) __INTMAX_C(x)
#   undef UINTMAX_C
#   define UINTMAX_C(x) __UINTMAX_C(x)
#endif

#ifdef __UINT8_MAX__
#   undef UINT8_MAX
#   define UINT8_MAX __UINT8_MAX__
#endif
#ifdef __UINT16_MAX__
#   undef UINT16_MAX
#   define UINT16_MAX __UINT16_MAX__
#endif
#ifdef __UINT32_MAX__
#   undef UINT32_MAX
#   define UINT32_MAX __UINT32_MAX__
#endif
#ifdef __UINT64_MAX__
#   undef UINT64_MAX
#   define UINT64_MAX __UINT64_MAX__
#endif

#ifdef __INT8_MAX__
#   undef INT8_MAX
#   define INT8_MAX __INT8_MAX__
#endif
#ifdef __INT16_MAX__
#   undef INT16_MAX
#   define INT16_MAX __INT16_MAX__
#endif
#ifdef __INT32_MAX__
#   undef INT32_MAX
#   define INT32_MAX __INT32_MAX__
#endif
#ifdef __INT64_MAX__
#   undef INT64_MAX
#   define INT64_MAX __INT64_MAX__
#endif

#ifdef __INT8_MAX__
#   undef INT8_MIN
#   define INT8_MIN (-INT8_MAX - 1)
#endif
#ifdef __INT16_MAX__
#   undef INT16_MIN
#   define INT16_MIN (-INT16_MAX - 1)
#endif
#ifdef __INT32_MAX__
#   undef INT32_MIN
#   define INT32_MIN (-INT32_MAX - 1)
#endif
#ifdef __INT64_MAX__
#   undef INT64_MIN
#   define INT64_MIN (-INT64_MAX - 1)
#endif

#undef UINT_LEAST8_MAX
#define UINT_LEAST8_MAX __UINT_LEAST8_MAX__
#undef UINT_LEAST16_MAX
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#undef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#undef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__

#undef INT_LEAST8_MAX
#define INT_LEAST8_MAX __INT_LEAST8_MAX__
#undef INT_LEAST16_MAX
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#undef INT_LEAST32_MAX
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#undef INT_LEAST64_MAX
#define INT_LEAST64_MAX __INT_LEAST64_MAX__

#undef INT_LEAST8_MIN
#define INT_LEAST8_MIN (-INT_LEAST8_MAX - 1)
#undef INT_LEAST16_MIN
#define INT_LEAST16_MIN (-INT_LEAST16_MAX - 1)
#undef INT_LEAST32_MIN
#define INT_LEAST32_MIN (-INT_LEAST32_MAX - 1)
#undef INT_LEAST64_MIN
#define INT_LEAST64_MIN (-INT_LEAST64_MAX - 1)

#undef UINT_FAST8_MAX
#define UINT_FAST8_MAX __UINT_FAST8_MAX__
#undef UINT_FAST16_MAX
#define UINT_FAST16_MAX __UINT_FAST16_MAX__
#undef UINT_FAST32_MAX
#define UINT_FAST32_MAX __UINT_FAST32_MAX__
#undef UINT_FAST64_MAX
#define UINT_FAST64_MAX __UINT_FAST64_MAX__

#undef INT_FAST8_MAX
#define INT_FAST8_MAX __INT_FAST8_MAX__
#undef INT_FAST16_MAX
#define INT_FAST16_MAX __INT_FAST16_MAX__
#undef INT_FAST32_MAX
#define INT_FAST32_MAX __INT_FAST32_MAX__
#undef INT_FAST64_MAX
#define INT_FAST64_MAX __INT_FAST64_MAX__

#undef INT_FAST8_MIN
#define INT_FAST8_MIN (-INT_FAST8_MAX - 1)
#undef INT_FAST16_MIN
#define INT_FAST16_MIN (-INT_FAST16_MAX - 1)
#undef INT_FAST32_MIN
#define INT_FAST32_MIN (-INT_FAST32_MAX - 1)
#undef INT_FAST64_MIN
#define INT_FAST64_MIN (-INT_FAST64_MAX - 1)

#ifdef __UINTPTR_MAX__
#   undef UINTPTR_MAX
#   define UINTPTR_MAX __UINTPTR_MAX__
#endif
#ifdef __INTPTR_MAX__
#   undef INTPTR_MAX
#   define INTPTR_MAX __INTPTR_MAX__
#   undef INTPTR_MIN
#   define INTPTR_MIN (-INTPTR_MAX - 1)
#endif

#undef UINTMAX_MAX
#define UINTMAX_MAX __UINTMAX_MAX__
#undef INTMAX_MAX
#define INTMAX_MAX __INTMAX_MAX__
#undef INTMAX_MIN
#define INTMAX_MIN (-INTMAX_MAX - 1)

#undef PTRDIFF_MAX
#define PTRDIFF_MAX __PTRDIFF_MAX__
#undef PTRDIFF_MIN
#define PTRDIFF_MIN (-PTRDIFF_MAX - 1)

#undef SIG_ATOMIC_MAX
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__
#undef SIG_ATOMIC_MIN
#define SIG_ATOMIC_MIN (-SIG_ATOMIC_MAX - 1)

#undef SIZE_MAX
#define SIZE_MAX __SIZE_MAX__

#undef WCHAR_MAX
#define WCHAR_MAX __WCHAR_MAX__
#undef WCHAR_MIN
#define WCHAR_MIN (-WCHAR_MAX - 1)

#undef WINT_MAX
#define WINT_MAX __WINT_MAX__
#undef WINT_MIN
#define WINT_MIN (-WINT_MAX - 1)

#endif
