#include "term/klog.h"
#include "lib/sprintf.h"

void logk(const char *str) {
    kmsg_write(str);
    while (*str) {
        char ch = *str++;
#if defined(__x86_64__) || defined(__amd64__)
        extern void write_serial(char a);
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
