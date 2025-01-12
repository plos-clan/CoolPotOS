#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if defined(TERMINAL_EMBEDDED_FONT)
enum TerminalInitResult terminal_init(const struct TerminalDisplay *display,
                                      float font_size,
                                      void *(*malloc)(size_t),
                                      void (*free)(void*),
                                      void (*serial_print)(const char*));
#endif

#if !defined(TERMINAL_EMBEDDED_FONT)
enum TerminalInitResult terminal_init(const struct TerminalDisplay *display,
                                      const uint8_t *font_buffer,
                                      size_t font_buffer_size,
                                      float font_size,
                                      void *(*malloc)(size_t),
                                      void (*free)(void*),
                                      void (*serial_print)(const char*));
#endif

void terminal_destroy(void);

void terminal_flush(void);

void terminal_process(const char *s);

void terminal_process_char(char c);

void terminal_set_history_size(size_t size);

void terminal_set_nature_scroll(bool mode);

void terminal_set_auto_flush(bool auto_flush);

void terminal_set_auto_crnl(bool auto_crnl);

void terminal_set_bell_handler(void (*handler)(void));

void terminal_set_color_scheme(size_t palette_index);

void terminal_set_custom_color_scheme(const struct TerminalPalette *palette);

const char *terminal_handle_keyboard(uint8_t scancode);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
