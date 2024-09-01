#include "../include/math.h"
#include "../include/ctype.h"

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

double mod(double x, double y){
    return x - (int32_t)(x / y) * y;
}

double sin(double x){
    x  = mod(x, 2 * PI);
    double  sum  = x;
    double  term = x;
    int  n    = 1;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        n    += 2;
        term *= x * x / (n * (n - 1));
        sum  += sign ? -term : term;
        sign  = !sign;
    }
    return sum;
}

double cos(double x){
    x         = mod(x, 2 * PI);
    double  sum  = 1;
    double  term = 1;
    int  n    = 0;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        n    += 2;
        term *= x * x / (n * (n - 1));
        sum  += sign ? -term : term;
        sign  = !sign;
    }
    return sum;
}

double tan(double x) {
    return sin(x) / cos(x);
}

double asin(double x){
    double sum  = x;
    double term = x;
    int n    = 1;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        term *= (x * x * (2 * n - 1) * (2 * n - 1)) / (2 * n * (2 * n + 1));
        sum  += term;
        n++;
    }
    return sum;
}

double acos(double x) {
    return PI / 2 - asin(x);
}

double atan(double x){
    double  sum  = x;
    double  term = x;
    int  n    = 1;
    bool sign = true;
    while (term > F64_EPSILON || term < -F64_EPSILON) {
        term *= x * x * (2 * n - 1) / (2 * n + 1);
        sum  += sign ? -term : term;
        sign  = !sign;
        n++;
    }
    return sum;
}

double atan2(double y, double x){
    if (x > 0) return atan(y / x);
    if (x < 0 && y >= 0) return atan(y / x) + PI;
    if (x < 0 && y < 0) return atan(y / x) - PI;
    if (x == 0 && y > 0) return PI / 2;
    if (x == 0 && y < 0) return -PI / 2;
    return 0;
}
