#include "../include/math.h"

static unsigned long long rand_seed = 1 ;
static unsigned short max_bit = 32 ;
static unsigned short randlevel = 1 ;

const unsigned long long rand(){
    unsigned short i = 0;
    while(i < randlevel)
        rand_seed = rand_seed * 1103515245 + 12345 , rand_seed <<= max_bit , i ++ ;
    return (const unsigned long long)(rand_seed >>= max_bit) ;
}

void srand(unsigned long long seed){
    rand_seed = seed ;
}

void smax(unsigned short max_b){
    max_b = (sizeof(unsigned long long) * 8) - (max_b % (sizeof(unsigned long long) * 8)) ;
    max_bit = (max_b == 0) ? (sizeof(unsigned long long) * 8 / 2) : (max_b) ;
}

void srandlevel(unsigned short randlevel_){
    if(randlevel_ != 0)
        randlevel = randlevel_ ;
}

int32_t abs(int32_t x){
    return (x < 0) ? (-x) : (x) ;
}

double pow(double a,long long b){
    char t = 0 ;
    if(b < 0)b = -b , t = 1 ;
    double ans = 1 ;
    while(b){
        if(b & 1)ans *= a ;
        a *= a ;
        b >>= 1 ;
    }
    if(t)return (1.0 / ans) ;
    else return ans ;
}

//快速整数平方
unsigned long long ull_pow(unsigned long long a,unsigned long long b){
    unsigned long long ans = 1 ;
    while(b){
        if(b & 1)ans *= a ;
        a *= a ;
        b >>= 1 ;
    }
    return ans ;
}


double sqrt(double x){
    if(x == 0)return 0.0 ;
    double xk = 1.0,xk1 = 0.0 ;
    while(xk != xk1)xk1 = xk , xk = (xk + x / xk) / 2.0 ;
    return xk ;
}

//快速求算数平方根（速度快，精度低）
float q_sqrt(float number){
    long i ;
    float x,y ;
    const float f = 1.5F ;
    x = number * 0.5F ;
    y = number ;
    i = *(long*)(&y) ;
    i = 0x5f3759df - (i >> 1) ;
    y = *(float*)(&i) ;
    y = y * (f - (x * y * y)) ;
    y = y * (f - (x * y * y)) ;
    return number * y ;
}
