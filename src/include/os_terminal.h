#pragma once

#include "ctypes.h"
#include "klog.h"
#include "kmalloc.h"
#include "tty.h"
#include "video.h"

typedef struct TerminalPalette {
    uint32_t foreground;
    uint32_t background;
    uint32_t ansi_colors[16];
} TerminalPalette;

bool terminal_init(size_t width, size_t height, uint32_t *screen,
                   float font_size, void *(*malloc)(size_t),
                   void (*free)(void *), void (*serial_print)(const char *));
void terminal_destroy(void);
void terminal_flush(void);
void terminal_set_auto_flush(size_t auto_flush);
void terminal_advance_state(const char *s);
void terminal_advance_state_single(char c);
void terminal_set_color_scheme(size_t palette_index);
void terminal_set_custom_color_scheme(struct TerminalPalette palette);
bool terminal_handle_keyboard(uint8_t scancode, char *buffer);

static inline void terminal_setup() {
    extern uint32_t tty_status;
    tty_status = TTY_OST_OUTPUT;

    terminal_init(get_vbe_width(), get_vbe_height(), get_vbe_screen(), 10.0,
                  kmalloc, kfree, logk);
}

static inline void terminal_exit(){
    extern uint32_t tty_status;
    tty_status = TTY_LOG_OUTPUT;
    terminal_destroy();
}