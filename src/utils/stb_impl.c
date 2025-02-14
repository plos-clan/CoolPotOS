#define STB_SPRINTF_IMPLEMENTATION
#include "sprintf.h"

int sprintf(char *buf, char const *fmt, ...){
    int result;
    va_list va;
    va_start(va, fmt);
    result = STB_SPRINTF_DECORATE(vsprintfcb)(0, 0, buf, fmt, va);
    va_end(va);
    return result;
}
