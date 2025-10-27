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

#ifndef __FREESTND_C_HDRS_STDARG_H
#define __FREESTND_C_HDRS_STDARG_H 1

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

#endif
