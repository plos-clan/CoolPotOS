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

#include <stdint.h>

void srandlevel(unsigned short randlevel_);
void smax(unsigned short max_b);
void srand(unsigned long long seed);
const unsigned long long rand();
int32_t abs(int32_t x);
double pow(double a,long long b);
unsigned long long ull_pow(unsigned long long a,unsigned long long b);
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

#endif
