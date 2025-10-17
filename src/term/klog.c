#define STB_SPRINTF_IMPLEMENTATION
#include "term/klog.h"
#include "lib/sprintf.h"

void logk(const char *str) {
    while (*str) {
        char ch = *str++;
        kmsg_putc(ch);
    }
}

void logkf(char *fmt, ...) {
    char    buf[4096] = {0};
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    logk(buf);
}


