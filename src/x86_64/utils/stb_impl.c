#define STB_SPRINTF_IMPLEMENTATION
#include "lock.h"
#include "sprintf.h"

spin_t sprintf_lock;

int sprintf(char *buf, char const *fmt, ...) {
    spin_lock(sprintf_lock);
    int     result;
    va_list va;
    va_start(va, fmt);
    result = STB_SPRINTF_DECORATE(vsprintfcb)(0, 0, buf, fmt, va);
    va_end(va);
    spin_unlock(sprintf_lock);
    return result;
}
