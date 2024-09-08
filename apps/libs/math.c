#include "../include/math.h"
#include "../include/ctype.h"

static const double ivln10hi =
        4.34294481878168880939e-01, /* 0x3fdbcb7b, 0x15200000 */
ivln10lo = 2.50829467116452752298e-11,          /* 0x3dbb9438, 0xca9aadd5 */
log10_2hi = 3.01029995663611771306e-01,         /* 0x3FD34413, 0x509F6000 */
log10_2lo = 3.69423907715893078616e-13,         /* 0x3D59FEF3, 0x11F12B36 */
Lg1 = 6.666666666666735130e-01,                 /* 3FE55555 55555593 */
Lg2 = 3.999999999940941908e-01,                 /* 3FD99999 9997FA04 */
Lg3 = 2.857142874366239149e-01,                 /* 3FD24924 94229359 */
Lg4 = 2.222219843214978396e-01,                 /* 3FCC71C5 1D8E78AF */
Lg5 = 1.818357216161805012e-01,                 /* 3FC74664 96CB03DE */
Lg6 = 1.531383769920937332e-01,                 /* 3FC39A09 D078C69F */
Lg7 = 1.479819860511658591e-01;                 /* 3FC2F112 DF3E5244 */

#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
  __asm__ ("subl %5,%1\n\tsbbl %3,%0"                    \
       : "=r" ((USItype) (sh)),                    \
         "=&r" ((USItype) (sl))                    \
       : "0" ((USItype) (ah)),                    \
         "g" ((USItype) (bh)),                    \
         "1" ((USItype) (al)),                    \
         "g" ((USItype) (bl)))
#define umul_ppmm(w1, w0, u, v) \
  __asm__ ("mull %3"                            \
       : "=a" ((USItype) (w0)),                    \
         "=d" ((USItype) (w1))                    \
       : "%0" ((USItype) (u)),                    \
         "rm" ((USItype) (v)))
#define udiv_qrnnd(q, r, n1, n0, dv) \
  __asm__ ("divl %4"                            \
       : "=a" ((USItype) (q)),                    \
         "=d" ((USItype) (r))                    \
       : "0" ((USItype) (n0)),                    \
         "1" ((USItype) (n1)),                    \
         "rm" ((USItype) (dv)))
#define count_leading_zeros(count, x) \
  do {                                    \
    USItype __cbtmp;                            \
    __asm__ ("bsrl %1,%0"                        \
         : "=r" (__cbtmp) : "rm" ((USItype) (x)));            \
    (count) = __cbtmp ^ 31;                        \
  } while (0)

static unsigned rand_seed = 1;
static unsigned short max_bit = 32;
static unsigned short randlevel = 1;

#define __negdi2(a) (-(a))

static UDWtype __udivmoddi4(UDWtype n, UDWtype d, UDWtype *rp) {
    DWunion ww;
    DWunion nn, dd;
    DWunion rr;
    UWtype d0, d1, n0, n1, n2;
    UWtype q0, q1;
    UWtype b, bm;

    nn.ll = n;
    dd.ll = d;

    d0 = dd.s.low;
    d1 = dd.s.high;
    n0 = nn.s.low;
    n1 = nn.s.high;

#if !defined(UDIV_NEEDS_NORMALIZATION)
    if (d1 == 0) {
        if (d0 > n1) {
            /* 0q = nn / 0D */

            udiv_qrnnd(q0, n0, n1, n0, d0);
            q1 = 0;

            /* Remainder in n0.  */
        } else {
            /* qq = NN / 0d */

            if (d0 == 0)
                d0 = 1 / d0;    /* Divide intentionally by zero.  */

            udiv_qrnnd(q1, n1, 0, n1, d0);
            udiv_qrnnd(q0, n0, n1, n0, d0);

            /* Remainder in n0.  */
        }

        if (rp != 0) {
            rr.s.low = n0;
            rr.s.high = 0;
            *rp = rr.ll;
        }
    }

#else /* UDIV_NEEDS_NORMALIZATION */

        if (d1 == 0)
    {
      if (d0 > n1)
    {
      /* 0q = nn / 0D */

      count_leading_zeros (bm, d0);

      if (bm != 0)
        {
          /* Normalize, i.e. make the most significant bit of the
         denominator set.  */

          d0 = d0 << bm;
          n1 = (n1 << bm) | (n0 >> (W_TYPE_SIZE - bm));
          n0 = n0 << bm;
        }

      udiv_qrnnd (q0, n0, n1, n0, d0);
      q1 = 0;

      /* Remainder in n0 >> bm.  */
    }
      else
    {
      /* qq = NN / 0d */

      if (d0 == 0)
        d0 = 1 / d0;	/* Divide intentionally by zero.  */

      count_leading_zeros (bm, d0);

      if (bm == 0)
        {
          /* From (n1 >= d0) /\ (the most significant bit of d0 is set),
         conclude (the most significant bit of n1 is set) /\ (the
         leading quotient digit q1 = 1).

         This special case is necessary, not an optimization.
         (Shifts counts of W_TYPE_SIZE are undefined.)  */

          n1 -= d0;
          q1 = 1;
        }
      else
        {
          /* Normalize.  */

          b = W_TYPE_SIZE - bm;

          d0 = d0 << bm;
          n2 = n1 >> b;
          n1 = (n1 << bm) | (n0 >> b);
          n0 = n0 << bm;

          udiv_qrnnd (q1, n1, n2, n1, d0);
        }

      /* n1 != d0...  */

      udiv_qrnnd (q0, n0, n1, n0, d0);

      /* Remainder in n0 >> bm.  */
    }

      if (rp != 0)
    {
      rr.s.low = n0 >> bm;
      rr.s.high = 0;
      *rp = rr.ll;
    }
    }
#endif /* UDIV_NEEDS_NORMALIZATION */

    else {
        if (d1 > n1) {
            /* 00 = nn / DD */

            q0 = 0;
            q1 = 0;

            /* Remainder in n1n0.  */
            if (rp != 0) {
                rr.s.low = n0;
                rr.s.high = n1;
                *rp = rr.ll;
            }
        } else {
            /* 0q = NN / dd */

            count_leading_zeros(bm, d1);
            if (bm == 0) {
                /* From (n1 >= d1) /\ (the most significant bit of d1 is set),
               conclude (the most significant bit of n1 is set) /\ (the
               quotient digit q0 = 0 or 1).

               This special case is necessary, not an optimization.  */

                /* The condition on the next line takes advantage of that
               n1 >= d1 (true due to program flow).  */
                if (n1 > d1 || n0 >= d0) {
                    q0 = 1;
                    sub_ddmmss(n1, n0, n1, n0, d1, d0);
                } else
                    q0 = 0;

                q1 = 0;

                if (rp != 0) {
                    rr.s.low = n0;
                    rr.s.high = n1;
                    *rp = rr.ll;
                }
            } else {
                UWtype m1, m0;
                /* Normalize.  */

                b = W_TYPE_SIZE - bm;

                d1 = (d1 << bm) | (d0 >> b);
                d0 = d0 << bm;
                n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 = n0 << bm;

                udiv_qrnnd(q0, n1, n2, n1, d1);
                umul_ppmm(m1, m0, q0, d0);

                if (m1 > n1 || (m1 == n1 && m0 > n0)) {
                    q0--;
                    sub_ddmmss(m1, m0, m1, m0, d1, d0);
                }

                q1 = 0;

                /* Remainder in (n1n0 - m1m0) >> bm.  */
                if (rp != 0) {
                    sub_ddmmss(n1, n0, n1, n0, m1, m0);
                    rr.s.low = (n1 << b) | (n0 >> bm);
                    rr.s.high = n1 >> bm;
                    *rp = rr.ll;
                }
            }
        }
    }

    ww.s.low = q0;
    ww.s.high = q1;
    return ww.ll;
}

__attribute__((used)) long long __moddi3(long long u, long long v) {
    int c = 0;
    DWunion uu, vv;
    DWtype w;

    uu.ll = u;
    vv.ll = v;

    if (uu.s.high < 0) {
        c = ~c;
        uu.ll = __negdi2 (uu.ll);
    }
    if (vv.s.high < 0)
        vv.ll = __negdi2 (vv.ll);

    __udivmoddi4(uu.ll, vv.ll, (UDWtype *) &w);
    if (c)
        w = __negdi2 (w);
    return w;
}

int rand() {
    unsigned short i = 0;
    while (i < randlevel)
        rand_seed = rand_seed * 1103515245 + 12345, rand_seed <<= max_bit, i++;
    return (rand_seed >>= max_bit);
}

void srand(unsigned seed) {
    rand_seed = seed;
}

void smax(unsigned short max_b) {
    max_b = (sizeof(unsigned long long) * 8) - (max_b % (sizeof(unsigned long long) * 8));
    max_bit = (max_b == 0) ? (sizeof(unsigned long long) * 8 / 2) : (max_b);
}

void srandlevel(unsigned short randlevel_) {
    if (randlevel_ != 0)
        randlevel = randlevel_;
}

int32_t abs(int32_t x) {
    return (x < 0) ? (-x) : (x);
}

double pow(double a, long long b) {
    char t = 0;
    if (b < 0)b = -b, t = 1;
    double ans = 1;
    while (b) {
        if (b & 1)ans *= a;
        a *= a;
        b >>= 1;
    }
    if (t)return (1.0 / ans);
    else return ans;
}

//快速整数平方
unsigned long long ull_pow(unsigned long long a, unsigned long long b) {
    unsigned long long ans = 1;
    while (b) {
        if (b & 1)ans *= a;
        a *= a;
        b >>= 1;
    }
    return ans;
}


double sqrt(double x) {
    if (x == 0)return 0.0;
    double xk = 1.0, xk1 = 0.0;
    while (xk != xk1)xk1 = xk, xk = (xk + x / xk) / 2.0;
    return xk;
}

//快速求算数平方根（速度快，精度低）
float q_sqrt(float number) {
    long i;
    float x, y;
    const float f = 1.5F;
    x = number * 0.5F;
    y = number;
    i = *(long *) (&y);
    i = 0x5f3759df - (i >> 1);
    y = *(float *) (&i);
    y = y * (f - (x * y * y));
    y = y * (f - (x * y * y));
    return number * y;
}

double mod(double x, double y) {
    return x - (int32_t)(x / y) * y;
}

double sin(double x) {
    x = mod(x, 2 * PI);
    double sum = x;
    double term = x;
    int n = 1;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        n += 2;
        term *= x * x / (n * (n - 1));
        sum += sign ? -term : term;
        sign = !sign;
    }
    return sum;
}

double cos(double x) {
    x = mod(x, 2 * PI);
    double sum = 1;
    double term = 1;
    int n = 0;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        n += 2;
        term *= x * x / (n * (n - 1));
        sum += sign ? -term : term;
        sign = !sign;
    }
    return sum;
}

double tan(double x) {
    return sin(x) / cos(x);
}

double asin(double x) {
    double sum = x;
    double term = x;
    int n = 1;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        term *= (x * x * (2 * n - 1) * (2 * n - 1)) / (2 * n * (2 * n + 1));
        sum += term;
        n++;
    }
    return sum;
}

double acos(double x) {
    return PI / 2 - asin(x);
}

double atan(double x) {
    double sum = x;
    double term = x;
    int n = 1;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        term *= x * x * (2 * n - 1) / (2 * n + 1);
        sum += sign ? -term : term;
        sign = !sign;
        n++;
    }
    return sum;
}

double atan2(double y, double x) {
    if (x > 0) return atan(y / x);
    if (x < 0 && y >= 0) return atan(y / x) + PI;
    if (x < 0 && y < 0) return atan(y / x) - PI;
    if (x == 0 && y > 0) return PI / 2;
    if (x == 0 && y < 0) return -PI / 2;
    return 0;
}

double floor(double x) {
    int flag = 1;
    if (fabs(x) == x) {
        flag = 0;
    }
    if (flag) {
        double r = (x); // 如果小于，则返回x-1的整数部分
        double s;
        modf(r, &s);
        return s - 1;
    } else {
        double r = (x); // 如果大于等于，则返回x的整数部分
        double s;
        modf(r, &s);
        return s;
    }
}

double modf(double x, double *iptr) {
    union {
        double f;
        uint64_t i;
    } u = {x};
    uint64_t mask;
    int e = (int) (u.i >> 52 & 0x7ff) - 0x3ff;

    /* no fractional part */
    if (e >= 52) {
        *iptr = x;
        if (e == 0x400 && u.i << 12 != 0) /* nan */
            return x;
        u.i &= 1ULL << 63;
        return u.f;
    }

    /* no integral part*/
    if (e < 0) {
        u.i &= 1ULL << 63;
        *iptr = u.f;
        return x;
    }

    mask = -1ULL >> 12 >> e;
    if ((u.i & mask) == 0) {
        *iptr = x;
        u.i &= 1ULL << 63;
        return u.f;
    }
    u.i &= ~mask;
    *iptr = u.f;
    return x - u.f;
}

double fabs(double x) {
    union {
        double f;
        uint64_t i;
    } u = {x};
    u.i &= -1ULL / 2;
    return u.f;
}

double ceil(double x) {
    int flag = 0;
    if (fabs(x) == x) {
        flag = 1;
    }
    if (flag) {
        double r = (x); // 如果小于，则返回x-1的整数部分
        double s;
        modf(r, &s);
        return s + 1;
    } else {
        double r = (x); // 如果大于等于，则返回x的整数部分
        double s;
        modf(r, &s);
        return s;
    }
}

#define __negdi2(a) (-(a))

long long __divdi3(long long u, long long v) {
    int c = 0;
    DWunion uu, vv;
    DWtype w;

    uu.ll = u;
    vv.ll = v;

    if (uu.s.high < 0) {
        c = ~c;
        uu.ll = __negdi2 (uu.ll);
    }
    if (vv.s.high < 0) {
        c = ~c;
        vv.ll = __negdi2 (vv.ll);
    }
    w = __udivmoddi4(uu.ll, vv.ll, (UDWtype *) 0);
    if (c)
        w = __negdi2 (w);
    return w;
}

double frexp(double x, int *e) {
    union {
        double d;
        uint64_t i;
    } y = {x};
    int ee = y.i >> 52 & 0x7ff;

    if (!ee) {
        if (x) {
            x = frexp(x * 0x1p64, e);
            *e -= 64;
        } else
            *e = 0;
        return x;
    } else if (ee == 0x7ff) {
        return x;
    }

    *e = ee - 0x3fe;
    y.i &= 0x800fffffffffffffull;
    y.i |= 0x3fe0000000000000ull;
    return y.d;
}

double scalbn(double x, int n) {
    union {
        double f;
        uint64_t i;
    } u;
    double y = x;

    if (n > 1023) {
        y *= 0x1p1023;
        n -= 1023;
        if (n > 1023) {
            y *= 0x1p1023;
            n -= 1023;
            if (n > 1023)
                n = 1023;
        }
    } else if (n < -1022) {
        y *= 0x1p-1022;
        n += 1022;
        if (n < -1022) {
            y *= 0x1p-1022;
            n += 1022;
            if (n < -1022)
                n = -1022;
        }
    }
    u.i = (uint64_t)(0x3ff + n) << 52;
    x = y * u.f;
    return x;
}

double scalbln(double x, long n) {
    if (n > INT_MAX)
        n = INT_MAX;
    else if (n < INT_MIN)
        n = INT_MIN;
    return scalbn(x, n);
}

double ldexp(double x, int n) { return scalbn(x, n); }

float scalbnf(float x, int n) {
    union {
        float f;
        uint32_t i;
    } u;
    float y = x;

    if (n > 127) {
        y *= 0x1p127f;
        n -= 127;
        if (n > 127) {
            y *= 0x1p127f;
            n -= 127;
            if (n > 127)
                n = 127;
        }
    } else if (n < -126) {
        y *= 0x1p-126f * 0x1p24f;
        n += 126 - 24;
        if (n < -126) {
            y *= 0x1p-126f * 0x1p24f;
            n += 126 - 24;
            if (n < -126)
                n = -126;
        }
    }
    u.i = (uint32_t)(0x7f + n) << 23;
    x = y * u.f;
    return x;
}

double fmod(double x, double y) {
    return x - (x / y) * y;
    /*
    union {
        double f;
        uint64_t i;
    } ux = {x}, uy = {y};
    int ex = ux.i >> 52 & 0x7ff;
    int ey = uy.i >> 52 & 0x7ff;
    int sx = ux.i >> 63;
    uint64_t i;

    uint64_t uxi = ux.i;

    if (uy.i << 1 == 0 || isnan(y) || ex == 0x7ff)
        return (x * y) / (x * y);
    if (uxi << 1 <= uy.i << 1) {
        if (uxi << 1 == uy.i << 1)
            return 0 * x;
        return x;
    }

    if (!ex) {
        for (i = uxi << 12; i >> 63 == 0; ex--, i <<= 1);
        uxi <<= -ex + 1;
    } else {
        uxi &= -1ULL >> 12;
        uxi |= 1ULL << 52;
    }
    if (!ey) {
        for (i = uy.i << 12; i >> 63 == 0; ey--, i <<= 1);
        uy.i <<= -ey + 1;
    } else {
        uy.i &= -1ULL >> 12;
        uy.i |= 1ULL << 52;
    }

    for (; ex > ey; ex--) {
        i = uxi - uy.i;
        if (i >> 63 == 0) {
            if (i == 0)
                return 0 * x;
            uxi = i;
        }
        uxi <<= 1;
    }
    i = uxi - uy.i;
    if (i >> 63 == 0) {
        if (i == 0)
            return 0 * x;
        uxi = i;
    }
    for (; uxi >> 52 == 0; uxi <<= 1, ex--);

    if (ex > 0) {
        uxi -= 1ULL << 52;
        uxi |= (uint64_t) ex << 52;
    } else {
        uxi >>= -ex + 1;
    }
    uxi |= (uint64_t) sx << 63;
    ux.i = uxi;
    return ux.f;
    */
}

double exp(double x) {

    x = 1.0 + x / 256;

    x *= x;
    x *= x;
    x *= x;
    x *= x;

    x *= x;
    x *= x;
    x *= x;
    x *= x;

    return x;

}

double log10(double x) {
    union {
        double f;
        uint64_t i;
    } u = {x};
    double hfsq, f, s, z, R, w, t1, t2, dk, y, hi, lo, val_hi, val_lo;
    uint32_t hx;
    int k;

    hx = u.i >> 32;
    k = 0;
    if (hx < 0x00100000 || hx >> 31) {
        if (u.i << 1 == 0)
            return -1 / (x * x); /* log(+-0)=-inf */
        if (hx >> 31)
            return (x - x) / 0.0; /* log(-#) = NaN */
        /* subnormal number, scale x up */
        k -= 54;
        x *= 0x1p54;
        u.f = x;
        hx = u.i >> 32;
    } else if (hx >= 0x7ff00000) {
        return x;
    } else if (hx == 0x3ff00000 && u.i << 32 == 0)
        return 0;

    /* reduce x into [sqrt(2)/2, sqrt(2)] */
    hx += 0x3ff00000 - 0x3fe6a09e;
    k += (int) (hx >> 20) - 0x3ff;
    hx = (hx & 0x000fffff) + 0x3fe6a09e;
    u.i = (uint64_t) hx << 32 | (u.i & 0xffffffff);
    x = u.f;

    f = x - 1.0;
    hfsq = 0.5 * f * f;
    s = f / (2.0 + f);
    z = s * s;
    w = z * z;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    R = t2 + t1;

    /* See log2.c for details. */
    /* hi+lo = f - hfsq + s*(hfsq+R) ~ log(1+f) */
    hi = f - hfsq;
    u.f = hi;
    u.i &= (uint64_t) - 1 << 32;
    hi = u.f;
    lo = f - hi - hfsq + s * (hfsq + R);

    /* val_hi+val_lo ~ log10(1+f) + k*log10(2) */
    val_hi = hi * ivln10hi;
    dk = k;
    y = dk * log10_2hi;
    val_lo = dk * log10_2lo + (lo + hi) * ivln10lo + lo * ivln10hi;

    /*
     * Extra precision in for adding y is not strictly needed
     * since there is no very large cancellation near x = sqrt(2) or
     * x = 1/sqrt(2), but we do it anyway since it costs little on CPUs
     * with some parallelism and it reduces the error for many args.
     */
    w = y + val_hi;
    val_lo += (y - w) + val_hi;
    val_hi = w;

    return val_lo + val_hi;
}

double log2(float x) {
    long *a;
    double o;
    a = (long *) &x;
    o = (double) *a;
    o = o / POW223 - 126.928071372;
    return o;
}

double log(double a) {
    int N = 15;//我们取了前15+1项来估算
    int k, nk;
    double x, xx, y;
    x = (a - 1) / (a + 1);
    xx = x * x;
    nk = 2 * N + 1;
    y = 1.0 / nk;
    for (k = N; k > 0; k--) {
        nk = nk - 2;
        y = 1.0 / nk + xx * y;

    }
    return 2.0 * x * y;

}

unsigned long long __udivdi3(unsigned long long u, unsigned long long v) {
    return __udivmoddi4(u, v, (UDWtype *) 0);
}

unsigned long long __umoddi3(unsigned long long u, unsigned long long v) {
    UDWtype w;

    __udivmoddi4(u, v, &w);
    return w;
}