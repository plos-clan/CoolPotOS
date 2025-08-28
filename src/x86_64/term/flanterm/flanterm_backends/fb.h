/* Copyright (C) 2022-2025 mintsuki and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FLANTERM_FB_H
#define FLANTERM_FB_H 1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../flanterm.h"

#ifdef FLANTERM_IN_FLANTERM

#include "fb_private.h"

#endif

struct flanterm_context *flanterm_fb_init(
    /* If _malloc and _free are nulled, use the bump allocated instance (1 use only). */
    void *(*_malloc)(size_t size),
    void (*_free)(void *ptr, size_t size),
    uint32_t *framebuffer, size_t width, size_t height, size_t pitch,
    uint8_t red_mask_size, uint8_t red_mask_shift,
    uint8_t green_mask_size, uint8_t green_mask_shift,
    uint8_t blue_mask_size, uint8_t blue_mask_shift,
    uint32_t *canvas, /* If nulled, no canvas. */
    uint32_t *ansi_colours, uint32_t *ansi_bright_colours, /* If nulled, default. */
    uint32_t *default_bg, uint32_t *default_fg, /* If nulled, default. */
    uint32_t *default_bg_bright, uint32_t *default_fg_bright, /* If nulled, default. */
    /* If font is null, use default font and font_width and font_height ignored. */
    void *font, size_t font_width, size_t font_height, size_t font_spacing,
    /* If scale_x and scale_y are 0, automatically scale font based on resolution. */
    size_t font_scale_x, size_t font_scale_y,
    size_t margin
);

#ifdef __cplusplus
}
#endif

#endif
