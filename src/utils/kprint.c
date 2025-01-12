#include "kprint.h"
#include "krlibc.h"
#include "os_terminal.h"
#include "sprintf.h"

static char *const color_codes[] = {
    [BLACK] = "0",
    [RED] = "1",
    [GREEN] = "2",
    [YELLOW] = "3",
    [BLUE] = "4",
    [MAGENTA] = "5",
    [CYAN] = "6",
    [WHITE] = "7"
};

void add_color(char *dest, uint32_t color, int is_background) {
    strcat(dest, "\033[");
    strcat(dest, is_background ? "4" : "3");
    strcat(dest, color_codes[color]);
    strcat(dest, "m");
}

void color_printk(size_t fcolor, size_t bcolor, const char *fmt, ...) {
    char buf[4096] = {0};
    add_color(buf, fcolor, false);
    add_color(buf, bcolor, true);

    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf + 11, fmt, args);
    va_end(args);

    strcat(buf, buf + 11);
    strcat(buf, "\033[0m");

    terminal_process(buf);
}
