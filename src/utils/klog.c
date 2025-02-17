#include <stdarg.h>
#include "klog.h"
#include "krlibc.h"
#include "serial.h"
#include "sprintf.h"

#define HARDWARE

void logk(const char *str) {
    while (*str){
        char ch = *str++;
#ifndef HARDWARE
        write_serial(ch);
#endif
    }
}

void logkf(char *fmt, ...) {
    char buf[4096] = {0};
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    logk(buf);
}
