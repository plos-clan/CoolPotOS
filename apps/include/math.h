#ifndef CRASHPOWEROS_MATH_H
#define CRASHPOWEROS_MATH_H

#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))

#define F32_EPSILON 1e-5f
#define F64_EPSILON 1e-10

#define PI 3.14159265358979323846264338327950288
#define E 2.718281828459045235360287

#define SQRT2 1.41421356237309504880168872420969807

#define PHI 1.61803398874989484820458683436563811772030917980576

#define NAN                        __builtin_nan("")
#define INFINITY                __builtin_inf()
#define    HUGE_VALF                __builtin_huge_valf()
#define    HUGE_VAL                __builtin_huge_val()
#define    HUGE_VALL                __builtin_huge_vall()

#define INT_MAX 2147483647
#define INT_MIN -2147483648

#define W_TYPE_SIZE   32
#define BITS_PER_UNIT 8

#define POW223    8388608.0

static inline unsigned __FLOAT_BITS(float __f) {
    union {
        float __f;
        unsigned __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}

static inline unsigned long long __DOUBLE_BITS(double __f) {
    union {
        double __f;
        unsigned long long __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}

#define fpclassify(x) ( \
    sizeof(x) == sizeof(float) ? __fpclassifyf(x) : \
    sizeof(x) == sizeof(double) ? __fpclassify(x) : \
    __fpclassify(x) )

#define isinf(x) ( \
    sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) == 0x7f800000 : \
    sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) == 0x7ffULL<<52 : \
    __fpclassify(x) == FP_INFINITE)

#define isnan(x) ( \
    sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 : \
    sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 : \
    __fpclassify(x) == FP_NAN)

#define isnormal(x) ( \
    sizeof(x) == sizeof(float) ? ((__FLOAT_BITS(x)+0x00800000) & 0x7fffffff) >= 0x01000000 : \
    sizeof(x) == sizeof(double) ? ((__DOUBLE_BITS(x)+(1ULL<<52)) & -1ULL>>1) >= 1ULL<<53 : \
    __fpclassify(x) == FP_NORMAL)

#define isfinite(x) ( \
    sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) < 0x7f800000 : \
    sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) < 0x7ffULL<<52 : \
    __fpclassify(x) > FP_INFINITE)

typedef int Wtype;
typedef unsigned int UWtype;
typedef unsigned int USItype;
typedef long long DWtype;
typedef unsigned long long UDWtype;

struct DWstruct {
    Wtype low, high;
};

typedef union {
    struct DWstruct s;
    DWtype ll;
} DWunion;

typedef long double XFtype;

#include <stdint.h>

void srandlevel(unsigned short randlevel_);

void smax(unsigned short max_b);

int32_t abs(int32_t x);

double pow(double a, long long b);

unsigned long long ull_pow(unsigned long long a, unsigned long long b);

double sqrt(double x);

float q_sqrt(float number);

double mod(double x, double y);

double sin(double x);

double cos(double x);

double tan(double x);

double asin(double x);

double acos(double x);

double atan(double x);

double atan2(double y, double x);

double floor(double x);

double modf(double x, double *iptr);

double fabs(double x);

double ceil(double x);

double frexp(double x, int *e);

double scalbln(double x, long n);

double ldexp(double x, int n);

float scalbnf(float x, int n);

double scalbn(double x, int n);

double fmod(double x, double y);

double log10(double x);

double log2(float x);

double log(double a);

double exp(double x);

float roundf(float x);

#endif
