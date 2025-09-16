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

#ifndef FLANTERM_H
#define FLANTERM_H 1

#include "ctype.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLANTERM_CB_DEC           10
#define FLANTERM_CB_BELL          20
#define FLANTERM_CB_PRIVATE_ID    30
#define FLANTERM_CB_STATUS_REPORT 40
#define FLANTERM_CB_POS_REPORT    50
#define FLANTERM_CB_KBD_LEDS      60
#define FLANTERM_CB_MODE          70
#define FLANTERM_CB_LINUX         80

#define FLANTERM_OOB_OUTPUT_OCRNL  (1 << 0)
#define FLANTERM_OOB_OUTPUT_OFDEL  (1 << 1)
#define FLANTERM_OOB_OUTPUT_OFILL  (1 << 2)
#define FLANTERM_OOB_OUTPUT_OLCUC  (1 << 3)
#define FLANTERM_OOB_OUTPUT_ONLCR  (1 << 4)
#define FLANTERM_OOB_OUTPUT_ONLRET (1 << 5)
#define FLANTERM_OOB_OUTPUT_ONOCR  (1 << 6)
#define FLANTERM_OOB_OUTPUT_OPOST  (1 << 7)

#ifdef FLANTERM_IN_FLANTERM

#    include "flanterm_private.h"

#else

struct flanterm_context;

#endif

void flanterm_putchar(struct flanterm_context *ctx, uint8_t c);
void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count);
void flanterm_flush(struct flanterm_context *ctx);
void flanterm_full_refresh(struct flanterm_context *ctx);
void flanterm_deinit(struct flanterm_context *ctx, void (*_free)(void *ptr, size_t size));

void     flanterm_get_dimensions(struct flanterm_context *ctx, size_t *cols, size_t *rows);
void     flanterm_set_autoflush(struct flanterm_context *ctx, bool state);
void     flanterm_set_callback(struct flanterm_context *ctx,
                               void (*callback)(struct flanterm_context *, uint64_t, uint64_t, uint64_t,
                                            uint64_t));
uint64_t flanterm_get_oob_output(struct flanterm_context *ctx);
void     flanterm_set_oob_output(struct flanterm_context *ctx, uint64_t oob_output);

#ifdef __cplusplus
}
#endif

#endif
