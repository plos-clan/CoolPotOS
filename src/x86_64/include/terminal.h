#pragma once

#include "ctype.h"
#include "heap.h"
#include "limine.h"

#define TERMINAL_EMBEDDED_FONT
#include "os_terminal.h"

void   init_terminal();
void   update_terminal();
void   terminal_putc(char c);
void   terminal_puts(const char *msg);
void   terminal_open_flush();
void   terminal_pty_writer(const uint8_t *data);
void   terminal_close_flush();
float  get_terminal_font_size();
size_t get_terminal_row(size_t height);
size_t get_terminal_col(size_t width);
