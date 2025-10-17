#define STB_SPRINTF_IMPLEMENTATION
#include "types.h"
#include "lib/sprintf.h"
#include "krlibc.h"

int sprintf(char *buf, char const *fmt, ...) {
    int     result;
    va_list va;
    va_start(va, fmt);
    result = STB_SPRINTF_DECORATE(vsprintfcb)(0, 0, buf, fmt, va);
    va_end(va);
    return result;
}

int snprintf(char *buf, int count, const char *fmt, ...) {
    int     result;
    va_list va;
    va_start(va, fmt);
    result = STB_SPRINTF_DECORATE(vsnprintf)(buf, count, fmt, va);
    va_end(va);
    return result;
}