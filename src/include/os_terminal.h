#pragma once

#include "ctypes.h"
#include "klog.h"
#include "kmalloc.h"
#include "tty.h"
#include "video.h"

typedef enum TerminalInitResult {
    Success,
    MallocIsNull,
    FreeIsNull,
    FontBufferIsNull,
} TerminalInitResult;

typedef struct TerminalDisplay {
    size_t width;
    size_t height;
    uint32_t *address;
} TerminalDisplay;

typedef struct TerminalPalette {
    uint32_t foreground;
    uint32_t background;
    uint32_t ansi_colors[16];
} TerminalPalette;

enum TerminalInitResult terminal_init(const struct TerminalDisplay *display,
                                      float font_size, void *(*malloc)(size_t),
                                      void (*free)(void *),
                                      void (*serial_print)(const char *));

void terminal_destroy(void);
void terminal_flush(void);
void terminal_set_auto_flush(size_t auto_flush);
void terminal_process(const char *s);
void terminal_process_char(char c);
void terminal_set_bell_handler(void (*handler)(void));
void terminal_set_history_size(size_t size);
void terminal_set_color_scheme(size_t palette_index);
void terminal_set_custom_color_scheme(struct TerminalPalette palette);
bool terminal_handle_keyboard(uint8_t scancode, char *buffer);
void terminal_set_nature_scroll(bool mode);

static inline void terminal_setup() {
    extern uint32_t tty_status;
    tty_status = TTY_OST_OUTPUT;

    TerminalDisplay display = {.width = get_vbe_width(),
                               .height = get_vbe_height(),
                               .address = get_vbe_screen()};
    terminal_init(&display, 10.0, kmalloc, kfree, logk);
}

static inline void terminal_exit() {
    extern uint32_t tty_status;
    tty_status = TTY_LOG_OUTPUT;
    terminal_destroy();
}