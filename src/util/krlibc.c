#include "krlibc.h"
#include "video.h"
#include "kmalloc.h"
#include "acpi.h" //void sleep(u32 time)
#include "timer.h" //void sleep(u32 time)

#define ZEROPAD    1        /* pad with zero */
#define SIGN    2        /* unsigned/signed long */
#define PLUS    4        /* show plus */
#define SPACE    8        /* space if plus */
#define LEFT    16        /* left justified */
#define SMALL    32        /* Must be 32 == 0x20 */
#define SPECIAL    64        /* 0x */

#define __do_div(n, base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

static int skip_atoi(const char **s) {
    int i = 0;

    while (isdigit(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

static char *number(char *str, long num, int base, int size, int precision,
                    int type) {
    static const char digits[16] = "0123456789ABCDEF";

    char tmp[66];
    char c, sign, locase;
    int i;

    locase = (type & SMALL);
    if (type & LEFT)
        type &= ~ZEROPAD;
    if (base < 2 || base > 16)
        return NULL;
    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    if (type & SIGN) {
        if (num < 0) {
            sign = '-';
            num = -num;
            size--;
        } else if (type & PLUS) {
            sign = '+';
            size--;
        } else if (type & SPACE) {
            sign = ' ';
            size--;
        }
    }
    if (type & SPECIAL) {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0)
            tmp[i++] = (digits[__do_div(num, base)] | locase);
    if (i > precision)
        precision = i;
    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
        while (size-- > 0)
            *str++ = ' ';
    if (sign)
        *str++ = sign;
    if (type & SPECIAL) {
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = ('X' | locase);
        }
    }
    if (!(type & LEFT))
        while (size-- > 0)
            *str++ = c;
    while (i < precision--)
        *str++ = '0';
    while (i-- > 0)
        *str++ = tmp[i];
    while (size-- > 0)
        *str++ = ' ';
    return str;
}

static const double rounders[10 + 1] = {
        0.5,                // 0
        0.05,                // 1
        0.005,                // 2
        0.0005,                // 3
        0.00005,            // 4
        0.000005,            // 5
        0.0000005,            // 6
        0.00000005,            // 7
        0.000000005,        // 8
        0.0000000005,        // 9
        0.00000000005        // 10
};

int atoi(const char *pstr) {
    int Ret_Integer = 0;
    int Integer_sign = 1;

    if (pstr == NULL) {
        return 0;
    }
    while (isspace(*pstr) == 0) {
        pstr++;
    }
    if (*pstr == '-') {
        Integer_sign = -1;
    }
    if (*pstr == '-' || *pstr == '+') {
        pstr++;
    }
    while (*pstr >= '0' && *pstr <= '9') {
        Ret_Integer = Ret_Integer * 10 + *pstr - '0';
        pstr++;
    }
    Ret_Integer = Integer_sign * Ret_Integer;

    return Ret_Integer;
}

char *ftoa(double f, char *buf, int precision) {
    char *ptr = buf;
    char *p = ptr;
    char *p1;
    char c;
    long intPart;

    if (precision > 10)
        precision = 10;
    if (f < 0) {
        f = -f;
        *ptr++ = '-';
    }
    if (precision < 0) {
        if (f < 1.0) precision = 6;
        else if (f < 10.0) precision = 5;
        else if (f < 100.0) precision = 4;
        else if (f < 1000.0) precision = 3;
        else if (f < 10000.0) precision = 2;
        else if (f < 100000.0) precision = 1;
        else precision = 0;
    }
    if (precision)
        f += rounders[precision];
    intPart = f;
    f -= intPart;
    if (!intPart)
        *ptr++ = '0';
    else {
        p = ptr;
        while (intPart) {
            *p++ = '0' + intPart % 10;
            intPart /= 10;
        }
        p1 = p;
        while (p > ptr) {
            c = *--p;
            *p = *ptr;
            *ptr++ = c;
        }
        ptr = p1;
    }
    if (precision) {
        *ptr++ = '.';
        while (precision--) {
            f *= 10.0;
            c = f;
            *ptr++ = '0' + c;
            f -= c;
        }
    }
    *ptr = 0;
    return ptr;
}

float ceilf(float x) {
    float fract = x - (int) x; // 获取小数部分
    if (fract > 0) {
        return (int) x + 1; // 如果有小数部分，向上取整
    } else {
        return (int) x; // 如果没有小数部分，直接返回整数部分
    }
}

float floorf(float x) {
    float fract = x - (int) x; // 获取小数部分
    if (fract < 0) {
        return (int) x - 1; // 如果是负数且有小数部分，向下取整
    } else {
        return (int) x; // 如果是正数或没有小数部分，直接返回整数部分
    }
}

float roundf(float number) {
    if (number < 0.0f) {
        return ceilf(number - 0.5f);
    } else {
        return floorf(number + 0.5f);
    }
}

double fabs(double x) {
    if (x < 0) {
        return -x;
    } else {
        return x;
    }
}

double floor(double x) {
    double fract = x - (int) x;
    if (fract < 0) {
        return (int) x - 1;
    } else {
        return (int) x;
    }
}

double ceil(double x) {
    double fract = x - (int) x; // 获取小数部分
    if (fract > 0) {
        return (int) x + 1; // 如果有小数部分，向上取整
    } else {
        return (int) x; // 如果没有小数部分，直接返回整数部分
    }
}

double fmod(double x, double y) {
    if (y == 0) {
        return __builtin_nanf("");
    }
    double intPart = x / y;
    double remainder = x - intPart * y;
    if (remainder < 0) {
        remainder += y;
    } else if (remainder > y) {
        remainder -= y;
    }
    return remainder;
}

double cos(double x) {
    double sum = 0.0;
    double term = 1.0;
    int n = 0;

    for (n = 0; term > 1e-15; n++) {
        term = term * (-1) * (2 * n) * (2 * n - 1) / ((2 * n) * (2 * n - 1));
        sum += term;
    }

    return sum;
}

double acos(double x) {
    double x0 = x;
    double x1;
    double tolerance = 1e-15;
    double max_iterations = 1000;
    int iterations = 0;

    while (iterations < max_iterations) {
        x1 = x0 - (-1 / sqrt(1 - x0 * x0)) / (1 / cos(x0));
        if (fabs(x1 - x0) < tolerance) {
            break;
        }
        x0 = x1;
        iterations++;
    }

    return x1;
}

double sqrt(double number) {
    if (number < 0) {
        return __builtin_nanf("");
    }

    double x = number;
    double epsilon = 1e-15;
    double diff;

    do {
        x = (x + number / x) / 2;
        diff = fabs(x - number / x);
    } while (diff > epsilon);

    return x;
}

double pow(double x, int y) {
    double result = 1.0;
    for (int i = 0; i < y; i++) {
        result *= x;
    }
    return result;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    int len;
    unsigned long num;
    int i, base;
    char *str;
    const char *s;

    int flags;        /* flags to number() */

    int field_width;    /* width of output field */
    int precision;        /* min. # of digits for integers; max
				   number of chars for from string */
    int qualifier;        /* 'h', 'l', or 'L' for integer fields */

    for (str = buf; *fmt; ++fmt) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        /* process flags */
        flags = 0;
        repeat:
        ++fmt;        /* this also skips first '%' */
        switch (*fmt) {
            case '-':
                flags |= LEFT;
                goto repeat;
            case '+':
                flags |= PLUS;
                goto repeat;
            case ' ':
                flags |= SPACE;
                goto repeat;
            case '#':
                flags |= SPECIAL;
                goto repeat;
            case '0':
                flags |= ZEROPAD;
                goto repeat;
        }

        /* get field width */
        field_width = -1;
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            ++fmt;
            /* it's the next argument */
            field_width = va_arg(args,
                                 int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (isdigit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                ++fmt;
                /* it's the next argument */
                precision = va_arg(args,
                                   int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        /* default base */
        base = 10;

        switch (*fmt) {
            case 'c':
                if (!(flags & LEFT))
                    while (--field_width > 0)
                        *str++ = ' ';
                *str++ = (unsigned char) va_arg(args,
                                                int);
                while (--field_width > 0)
                    *str++ = ' ';
                continue;

            case 's':
                s = va_arg(args,
                           char *);
                len = strnlen(s, precision);

                if (!(flags & LEFT))
                    while (len < field_width--)
                        *str++ = ' ';
                for (i = 0; i < len; ++i)
                    *str++ = *s++;
                while (len < field_width--)
                    *str++ = ' ';
                continue;

            case 'p':
                if (field_width == -1) {
                    field_width = 2 * sizeof(void *);
                    flags |= ZEROPAD;
                }
                str = number(str, (unsigned long) va_arg(args,
                                                         void *), 16,
                             field_width, precision, flags);
                continue;

            case 'n':
                if (qualifier == 'l') {
                    long *ip = va_arg(args,
                                      long *);
                    *ip = (str - buf);
                } else {
                    int *ip = va_arg(args,
                                     int *);
                    *ip = (str - buf);
                }
                continue;

            case '%':
                *str++ = '%';
                continue;

                /* integer number formats - set up the flags and "break" */
            case 'o':
                base = 8;
                break;

            case 'x':
                flags |= SMALL;
            case 'X':
                base = 16;
                break;

            case 'd':
            case 'i':
                flags |= SIGN;
            case 'u':
                break;
            case 'f':
                str = ftoa(va_arg(args, double), str, precision);
                break;

            default:
                *str++ = '%';
                if (*fmt)
                    *str++ = *fmt;
                else
                    --fmt;
                continue;
        }
        if (qualifier == 'l')
            num = va_arg(args,
                         unsigned long);
        else if (qualifier == 'h') {
            num = (unsigned short) va_arg(args,
                                          int);
            if (flags & SIGN)
                num = (short) num;
        } else if (flags & SIGN)
            num = va_arg(args,
                         int);
        else
            num = va_arg(args,
                         unsigned int);
        str = number(str, num, base, field_width, precision, flags);
    }
    *str = '\0';
    return str - buf;
}

void memclean(char *s, int len) {
    // 清理某个内存区域（全部置0）
    int i;
    for (i = 0; i != len; i++) {
        s[i] = 0;
    }
}

void *memcpy(void *s, const void *ct, size_t n) {
    if (NULL == s || NULL == ct || n <= 0)
        return NULL;
    while (n--)
        *(char *) s++ = *(char *) ct++;
    return s;
}

int memcmp(const void *a_, const void *b_, uint32_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n-- > 0)
        *p++ = c;
    return s;
}

void *memmove(void *dest, const void *src, size_t num) {
    void *ret = dest;
    if (dest < src) {
        while (num--)//前->后
        {
            *(char *) dest = *(char *) src;
            dest = (char *) dest + 1;
            src = (char *) src + 1;
        }
    } else {
        while (num--)//后->前
        {
            *((char *) dest + num) = *((char *) src + num);
        }
    }
    return ret;
}

size_t strnlen(const char *s, size_t maxlen) {
    const char *es = s;
    while (*es && maxlen) {
        es++;
        maxlen--;
    }
    return (es - s);
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

char *strchrnul(const char *s, int c) {
    while (*s) {
        if ((*s++) == c)
            break;
    }
    return (char *) s;
}

char *strtok(char *str, const char *delim) {
    static char *src = NULL;
    const char *indelim = delim;
    int flag = 1, index = 0;
    char *temp = NULL;
    if (str == NULL) {
        str = src;
    }
    for (; *str; str++) {
        delim = indelim;
        for (; *delim; delim++) {
            if (*str == *delim) {
                *str = 0;
                index = 1;
                break;
            }
        }
        if (*str != 0 && flag == 1) {
            temp = str;
            flag = 0;
        }
        if (*str != 0 && flag == 0 && index == 1) {
            src = str;
            return temp;
        }
    }
    src = str;
    return temp;
}

char *strstr(char *str1, char *str2) {
    if (str1 == 0 || str2 == 0)return 0;
    const char *temp = 0;
    const char *res = 0;
    while (*str1 != '\0') {
        temp = str1;
        res = str2;
        while (*temp == *res)++temp, ++res;
        if (*res == '\0')return str1;
        ++str1;
    }
    return 0;
}

char *strncpy(char *dest, const char *src, unsigned long long count) {
    if (dest == 0 || src == 0)return 0;
    char *ret = dest;
    while (count)*dest = *src, ++dest, ++src, --count;
    return ret;
}

char *strdup(const char *str) {
    if (str == NULL)
        return NULL;

    char *strat = (char *) str;
    int len = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *) kmalloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {}

    return ret - (len + 1);
}

void strtoupper(char *str) {
    while (*str != '\0') {
        if (*str >= 'a' && *str <= 'z') {
            *str -= 32;
        }
        str++;
    }
}

int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *) s1,
            *p2 = (const unsigned char *) s2;
    while (n-- > 0) {
        if (*p1 != *p2)
            return *p1 - *p2;
        if (*p1 == '\0')
            return 0;
        p1++, p2++;
    }
    return 0;
}

int64_t strtol(const char *str, char **endptr, int base) {
    const char *s;
    unsigned long acc;
    char c;
    unsigned long cutoff;
    int neg, any, cutlim;
    s = str;
    do {
        c = *s++;
    } while (isspace((unsigned char) c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') ||
         (s[1] >= 'A' && s[1] <= 'F') ||
         (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    cutoff = neg ? (unsigned long) -(LONG_MIN + LONG_MAX) + LONG_MAX
                 : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    for (;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (!any) {
        noconv:
        {}
    } else if (neg)
        acc = -acc;
    if (endptr != NULL)
        *endptr = (char *) (any ? s - 1 : str);
    return (acc);
}

int strcmp(const char *s1, const char *s2) {
    char is_equal = 1;

    for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++) {
        if (*s1 != *s2) {
            is_equal = 0;
            break;
        }
    }

    if (is_equal) {
        if (*s1 != '\0') {
            return 1;
        } else if (*s2 != '\0') {
            return -1;
        } else {
            return 0;
        }
    } else {
        return (int) (*s1 - *s2);
    }
}

char *strcpy(char *dest, const char *src) {
    do {
        *dest++ = *src++;
    } while (*src != 0);
    *dest = 0;
}

char *strcat(char *dest, const char *src) {
    char *temp = dest;
    while (*temp != '\0')
        temp++;
    while ((*temp++ = *src++) != '\0');
}

char *strchr(const char *str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return (char *) str;
        }
        str++;
    }
    return NULL;
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
            c == '\v');
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}

void sleep(uint32_t time) {
    clock_sleep(time);
}
