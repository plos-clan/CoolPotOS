#ifndef CRASHPOWEROS_MATH_H
#define CRASHPOWEROS_MATH_H

#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))

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

#endif
