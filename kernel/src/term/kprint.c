#include "term/kprint.h"
#include "stdarg.h"
#include "lib/sprintf.h"
#include "lock.h"
#include "driver/device.h"

static spin_t printk_lock = SPIN_INIT;
static char buf[4096]     = { 0 }; // 输入缓冲区

static char *const color_codes[] = { [BLACK] = "0", [RED] = "1",     [GREEN] = "2", [YELLOW] = "3",
                                     [BLUE] = "4",  [MAGENTA] = "5", [CYAN] = "6",  [WHITE] = "7" };

static void add_color(char *dest, const uint32_t color, const int is_background) {
    strcat(dest, "\033[");
    strcat(dest, is_background ? "4" : "3");
    strcat(dest, color_codes[color]);
    strcat(dest, "m");
}

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

    const device_t *tty = device_find(DEV_TTY, 0);
    if (tty) {
        device_write(tty->dev, buf, 0, len);
    }

    return len;
}

void color_printk(const size_t fcolor, const size_t bcolor, const char *fmt, ...) {
    spin_lock(printk_lock);
    memset(buf, 0, 4096);
    add_color(buf, fcolor, false);
    add_color(buf, bcolor, true);

    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf + 11, fmt, args);
    va_end(args);

    strcat(buf, buf + 11);
    strcat(buf, "\033[0m");

    spin_unlock(printk_lock);

    const device_t *tty = device_find(DEV_TTY, 0);
    if (tty) {
        device_write(tty->dev, buf, 0, strlen(buf));
    }
}
