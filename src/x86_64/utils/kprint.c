#include "kprint.h"
#include "klog.h"
#include "krlibc.h"
#include "lock.h"
#include "os_terminal.h"
#include "pcb.h"
#include "sprintf.h"
#include "tty.h"

spin_t print_lock;

static char *const color_codes[] = {[BLACK] = "0", [RED] = "1",     [GREEN] = "2", [YELLOW] = "3",
                                    [BLUE] = "4",  [MAGENTA] = "5", [CYAN] = "6",  [WHITE] = "7"};

void add_color(char *dest, uint32_t color, int is_background) {
    strcat(dest, "\033[");
    strcat(dest, is_background ? "4" : "3");
    strcat(dest, color_codes[color]);
    strcat(dest, "m");
}

void init_print_lock() {
    print_lock = SPIN_INIT;
}

void color_printk(size_t fcolor, size_t bcolor, const char *fmt, ...) {
    spin_lock(print_lock);
    char buf[4096] = {0};
    add_color(buf, fcolor, false);
    add_color(buf, bcolor, true);

    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf + 11, fmt, args);
    va_end(args);

    strcat(buf, buf + 11);
    strcat(buf, "\033[0m");

    tty_t *tty_dev =
        get_current_task() == NULL ? get_default_tty() : get_current_task()->parent_group->tty;

    tty_dev->print(tty_dev, buf);

    extern bool open_flush;
    if (!open_flush) tty_dev->flush(tty_dev);
    spin_unlock(print_lock);
}

void cp_printf(const char *fmt, ...) {
    spin_lock(print_lock);
    char buf[4096] = {0};
    add_color(buf, WHITE, false);
    add_color(buf, BLACK, true);

    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf + 11, fmt, args);
    va_end(args);

    strcat(buf, buf + 11);
    strcat(buf, "\033[0m");

    tty_t *tty_dev =
        get_current_task() == NULL ? get_default_tty() : get_current_task()->parent_group->tty;

    tty_dev->print(tty_dev, buf);

    spin_unlock(print_lock);
}
