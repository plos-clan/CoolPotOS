#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "os_terminal.h"
#include "pcb.h"
#include "sprintf.h"

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

    if (get_current_task() != NULL)
        get_current_task()->parent_group->tty->print(get_current_task()->parent_group->tty, buf);
    else {
        for (size_t i = 0; buf[i] != '\0'; i++) {
            if (buf[i] == '\n') terminal_process_byte('\r');
            terminal_process_byte(buf[i]);
        }
    }
    extern bool open_flush;
    if (!open_flush) terminal_flush();
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

    if (get_current_task() != NULL)
        get_current_task()->parent_group->tty->print(get_current_task()->parent_group->tty, buf);
    else
        terminal_process(buf);
    spin_unlock(print_lock);
}
