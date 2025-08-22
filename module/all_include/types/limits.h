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

#ifndef __FREESTND_C_HDRS_LIMITS_H
#define __FREESTND_C_HDRS_LIMITS_H 1

#undef CHAR_BIT
#define CHAR_BIT __CHAR_BIT__

#ifndef MB_LEN_MAX
#    define MB_LEN_MAX 1
#endif

#undef SCHAR_MAX
#define SCHAR_MAX __SCHAR_MAX__
#undef SCHAR_MIN
#define SCHAR_MIN (-SCHAR_MAX - 1)

#undef UCHAR_MAX
#if __SCHAR_MAX__ == __INT_MAX__
#    define UCHAR_MAX (SCHAR_MAX * 2U + 1U)
#else
#    define UCHAR_MAX (SCHAR_MAX * 2 + 1)
#endif

#ifdef __CHAR_UNSIGNED__
#    undef CHAR_MAX
#    define CHAR_MAX UCHAR_MAX
#    undef CHAR_MIN
#    if __SCHAR_MAX__ == __INT_MAX__
#        define CHAR_MIN 0U
#    else
#        define CHAR_MIN 0
#    endif
#else
#    undef CHAR_MAX
#    define CHAR_MAX SCHAR_MAX
#    undef CHAR_MIN
#    define CHAR_MIN SCHAR_MIN
#endif

#undef SHRT_MAX
#define SHRT_MAX __SHRT_MAX__
#undef SHRT_MIN
#define SHRT_MIN (-SHRT_MAX - 1)

#undef USHRT_MAX
#if __SHRT_MAX__ == __INT_MAX__
#    define USHRT_MAX (SHRT_MAX * 2U + 1U)
#else
#    define USHRT_MAX (SHRT_MAX * 2 + 1)
#endif

#undef INT_MAX
#define INT_MAX __INT_MAX__
#undef INT_MIN
#define INT_MIN (-INT_MAX - 1)

#undef UINT_MAX
#define UINT_MAX (INT_MAX * 2U + 1U)

#undef LONG_MAX
#define LONG_MAX __LONG_MAX__
#undef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)

#undef ULONG_MAX
#define ULONG_MAX (LONG_MAX * 2UL + 1UL)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

#    undef LLONG_MAX
#    define LLONG_MAX __LONG_LONG_MAX__
#    undef LLONG_MIN
#    define LLONG_MIN (-LLONG_MAX - 1LL)

#    undef ULLONG_MAX
#    define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)

#endif

#endif
