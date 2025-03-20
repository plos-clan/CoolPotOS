#pragma once

#include "ctypes.h"
#include "klog.h"
#include "kmalloc.h"
#include "tty.h"
#include "video.h"

#define TERMINAL_EMBEDDED_FONT

typedef enum TerminalInitResult {
    Success,
    MallocIsNull,
    FreeIsNull,
    FontBufferIsNull,
} TerminalInitResult;

typedef struct TerminalDisplay {
    size_t    width;
    size_t    height;
    uint32_t *address;
} TerminalDisplay;

typedef struct TerminalPalette {
    uint32_t foreground;
    uint32_t background;
    uint32_t ansi_colors[16];
} TerminalPalette;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if defined(TERMINAL_EMBEDDED_FONT)
enum TerminalInitResult terminal_init(const struct TerminalDisplay *display, float font_size,
                                      void *(*malloc)(size_t), void (*free)(void *),
                                      void (*serial_print)(const char *));
#endif

#if !defined(TERMINAL_EMBEDDED_FONT)
enum TerminalInitResult terminal_init(const struct TerminalDisplay *display,
                                      const uint8_t *font_buffer, size_t font_buffer_size,
                                      float font_size, void *(*malloc)(size_t),
                                      void (*free)(void *), void (*serial_print)(const char *));
#endif

void terminal_destroy(void);

void terminal_flush(void);

void terminal_process(const char *s);

void terminal_process_char(char c);

void terminal_set_history_size(size_t size);

void terminal_set_nature_scroll(bool mode);

void terminal_set_auto_flush(bool auto_flush);

void terminal_set_bell_handler(void (*handler)(void));

void terminal_set_color_scheme(size_t palette_index);

void terminal_set_custom_color_scheme(const struct TerminalPalette *palette);

const uint8_t *terminal_handle_keyboard(uint8_t scancode);

static inline void cpos_default_color() {
    TerminalPalette palette = {
        .background = 0x000000,
        .foreground = 0xc6c6c6,
        .ansi_colors =
            {
                          [0] = 0x000000,
                          [1] = 0xee6363,
                          [2] = 0x00FF00,
                          [3] = 0x00FFFF,
                          [4] = 0x0036ff,
                          [5] = 0xdf1200,
                          [6] = 0x00bcdf,
                          [7] = 0xabc990,

                          [8]  = 0x111111,
                          [9]  = 0x711712,
                          [10] = 0x2d522a,
                          [11] = 0xbcff00,
                          [12] = 0x0036ff,
                          [13] = 0xdf1200,
                          [14] = 0x00bcdf,
                          [15] = 0xabc990,
                          },
    };
    terminal_set_custom_color_scheme(&palette);
}

static inline void terminal_setup(bool is_f3) {
    extern uint32_t tty_status;
    tty_status = TTY_OST_OUTPUT;

    TerminalDisplay display = {
        .width = get_vbe_width(), .height = get_vbe_height(), .address = get_vbe_screen()};
    terminal_init(&display, is_f3 ? 30.0 : 10.0, kmalloc, kfree, logk);

    extern void beep();
    terminal_set_bell_handler(beep);

    if (!is_f3) {
        cpos_default_color();
        // terminal_set_color_scheme(0);
        return;
    }
    TerminalPalette palette = {
        .background = 0x020f01,
        .foreground = 0x88ae73,
        .ansi_colors =
            {
                          [0] = 0x000,
                          [1] = 0x600601,
                          [2] = 0x2d522a,
                          [3] = 0xbcff00,
                          [4] = 0x0036ff,
                          [5] = 0xdf1200,
                          [6] = 0x00bcdf,
                          [7] = 0xabc990,

                          [8]  = 0x111111,
                          [9]  = 0x711712,
                          [10] = 0x2d522a,
                          [11] = 0xbcff00,
                          [12] = 0x0036ff,
                          [13] = 0xdf1200,
                          [14] = 0x00bcdf,
                          [15] = 0xabc990,
                          },
    };
    terminal_set_custom_color_scheme(&palette);
}

static inline void terminal_exit() {
    extern uint32_t tty_status;
    tty_status = TTY_LOG_OUTPUT;
    terminal_destroy();
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
