#include "term/kprint.h"
#include "stdarg.h"
#include "lib/sprintf.h"
#include "lock.h"
#include "driver/tty.h"

static spin_t printk_lock = SPIN_INIT;
static char buf[4096]     = { 0 }; // 输入缓冲区

static void logk(const char *str) {
    extern void write_serial(const char ch);
    for (; *str != '\0'; str++) {
        write_serial(*str);
    }
}

void logkf(char *fmt, ...) {
    char buf[4096] = { 0 };
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    logk(buf);
}

int printk(const char *fmt, ...) {

    spin_lock(printk_lock);
    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(buf, sizeof(buf), fmt, args);

    va_end(args);
    if (len < 0) {
        spin_unlock(printk_lock);
        return len;
    }
    if ((size_t)len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }

    spin_unlock(printk_lock);

    tty_t *tty = get_kernel_session();
    
    return len;
}
