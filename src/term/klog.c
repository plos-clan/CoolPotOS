#include "klog.h"
#include "kmesg.h"
#include "sprintf.h"
#include "types/stdarg.h"

#if defined(__x86_64__)
#    include "serial.h"
#endif

void logk(const char *str) {
    while (*str) {
        char ch = *str++;
#if defined(__x86_64__)
        write_serial(ch);
#endif
    }
}

void logkf(char *fmt, ...) {
    char    buf[4096] = {0};
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    logk(buf);
    kmsg_write(buf);
}
