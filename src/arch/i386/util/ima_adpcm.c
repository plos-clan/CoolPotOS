#include "vsound.h"

static const int index_adjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static const int step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

void sound_ima_adpcm_encode(ImaAdpcmCtx *ctx, void *dst, const int16_t *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int delta = src[i] - ctx->prev_sample;
        int sb    = delta < 0 ? 8 : 0;
        delta     = delta < 0 ? -delta : delta;
        int code  = 4 * delta / step_table[ctx->index];
        if (code > 7) code = 7;
        ctx->index += index_adjust[code];
        if (ctx->index < 0) ctx->index = 0;
        if (ctx->index > 88) ctx->index = 88;
        ctx->prev_sample = src[i];
        if (i % 2 == 0) {
            ((uint8_t *)dst)[i / 2] = code | sb;
        } else {
            ((uint8_t *)dst)[i / 2] |= (code | sb) << 4;
        }
    }
}

void sound_ima_adpcm_decode(ImaAdpcmCtx *ctx, int16_t *dst, const void *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int  code  = i % 2 == 0 ? ((uint8_t *)src)[i / 2] & 0x0f : ((uint8_t *)src)[i / 2] >> 4;
        bool sb    = code & 8;
        code      &= 7;
        int delta  = (step_table[ctx->index] * code) / 4 + step_table[ctx->index] / 8;
        delta      = sb ? -delta : delta;
        ctx->prev_sample += delta;
        if (ctx->prev_sample > 32767) ctx->prev_sample = 32767;
        if (ctx->prev_sample < -32768) ctx->prev_sample = -32768;
        dst[i]      = ctx->prev_sample;
        ctx->index += index_adjust[code];
        if (ctx->index < 0) ctx->index = 0;
        if (ctx->index > 88) ctx->index = 88;
    }
}