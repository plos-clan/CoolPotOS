#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t width;
    size_t height;
    uint32_t *buffer;
    size_t pitch;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} TerminalDisplay;

typedef struct {
    uint32_t foreground;
    uint32_t background;
    uint32_t ansi_colors[16];
} TerminalPalette;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if defined(TERMINAL_EMBEDDED_FONT)
void *terminal_new(
    const TerminalDisplay *display, float font_size, void *(*malloc)(size_t), void (*free)(void *)
);
#endif

#if !defined(TERMINAL_EMBEDDED_FONT)
void *terminal_new(
    const TerminalDisplay *display,
    const uint8_t *font_buffer,
    size_t font_buffer_size,
    float font_size,
    void *(*malloc)(size_t),
    void (*free)(void *)
);
#endif

void terminal_destroy(void *terminal);

size_t terminal_rows(void *terminal);

size_t terminal_columns(void *terminal);

void terminal_flush(void *terminal);

void terminal_process(void *terminal, const char *s);

void terminal_process_byte(void *terminal, uint8_t c);

void terminal_handle_keyboard(void *terminal, uint8_t scancode);

void terminal_handle_mouse_scroll(void *terminal, ptrdiff_t delta);

void terminal_set_history_size(void *terminal, size_t size);

void terminal_set_color_cache_size(void *terminal, size_t size);

void terminal_set_scroll_speed(void *terminal, size_t speed);

void terminal_set_auto_flush(void *terminal, bool auto_flush);

void terminal_set_crnl_mapping(void *terminal, bool auto_crnl);

void terminal_set_custom_color_scheme(void *terminal, const TerminalPalette *palette);

void terminal_set_pty_writer(void *terminal, void (*writer)(const uint8_t *, size_t));

void terminal_set_clipboard(
    void *terminal, const char *(*get_fn)(void), void (*set_fn)(const char *)
);

void terminal_set_color_scheme(void *terminal, size_t palette_index);

void terminal_set_bell_handler(void *terminal, void (*handler)(void));

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
