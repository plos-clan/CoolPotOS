#include "../include/soundtest.h"
#include "../include/sb16.h"
#include "../include/printf.h"
#include "../include/timer.h"

#define buffer_len 32768
static const char l[] = "  QQffQQLLLfLLDDQQQfff  rrff``UU  QQDDQQLLLfLLDD<<f333  r\x98rf`r`U  "
                        "<<<9<LUUU\x80U\x80UL[[rLLL  rrff`frr  <<<93U999`9U3+--9&&&  rrff``UU";

static int s(const char *a, int b, int c) {
    return c / a[b];
}
static int d(int a) {
    return a & 1 ? -1 : 1;
}
static int e(int a, int b, int t) {
    return t >> 22 & 1 ? b : a;
}
static int r(int a, int t) {
    return 112 >> (a >> 20 & 7) & 1 ? t >> 14 & 15 : (t >> 17 & 1) * 8;
}
static int b(const char *a, int p, int t) {
    return (s(a, r(t, t) + p * 16, t << 2) * (254 >> (t >> 20 & 7) & 1)) & 20;
}
static int q(int a) {
    return 120 >> (a >> 20 & 7) & 1 ? 1 : (-a >> 16 & 1);
}
static int g(int t) {
    int p = 178 >> (t >> 19 & 7) & 1;
    int n =
            b("rrLr99rrUUrU99rULLyL<<yy``\x80`@@\x80`", p, t) * 2 +
            b("qqKq88qqTTqT88qTKKxK;;xx__\x7f_??\x7f_", p, t) +
            (s("rf[<r`L@\x98\x88yQ\x80\146UL", (t & 3) + (t >> 17 & 1) * 4 + p * 8, t << 2) & 7) *
            (t * d(t >> 16) >> 12 & 15 ^ e(0, 5, t)) * 3 / 4 +
            ((s("rf[L<9-\x1e&-3&-3-3\xab\x98\x90r`UH0+&9+&\x1d&+\x98\x88yfQL<(3<D3<3-(&"
                "09HL`ULU`r`UL@9",
                (t >> 13 & 31) + p * 32, t << 5 - (t >> 11 & 3)) |
              t >> 8) *
             q(t) &
             31) +
            (((s(l, t >> 14 & 127, t << 6) & s(l, t >> 14 & 127, (t * e(89 / 88, 499 / 498, t)) << 6)) *
              (63486 >> (t >> 15 & 15) & 1) * (102 >> (t >> 20 & 7) & 1)) &
             e(42, 32, t)) +
            ((((253507989 >> (t >> 6 & 31)) * (1 >> (t >> 11 & 3)) * (19593 >> (t >> 13 & 15) & 1) & 1) *
              50) +
             ((((t * t / 21 + t & (t >> 3)) | t >> 7 | t >> 8) - 7) * (3 >> (t >> 11 & 3) & 1) *
              (2450526224 >> (t >> 13 & 31) & 1) &
              31) *
             5 / 2) *
            (112 >> (t >> 20 & 7) & 1);
    return n;
}

static int       F, G;
static const int T = 125000;
static const int K = 10;

static uint8_t gen(int t) {
    if (t < (128 - F) * K) return 128 - t / K;
    t -= (128 - F) * K;
    if (t < T) return g(t);
    t -= T;
    if (t < (128 - G) * K) return G + t / K;
    return 128;
    // return g(t);
}

void sound_test() {

}

