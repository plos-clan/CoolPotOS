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

#ifndef __FREESTND_C_HDRS_STDALIGN_H
#define __FREESTND_C_HDRS_STDALIGN_H 1

#ifndef __cplusplus

#    if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
/* These do not need to be defined for C23+ */
#    else
#        undef alignas
#        define alignas _Alignas
#        undef alignof
#        define alignof _Alignof

#        undef __alignof_is_defined
#        define __alignof_is_defined 1
#        undef __alignas_is_defined
#        define __alignas_is_defined 1
#    endif

#endif

#endif
