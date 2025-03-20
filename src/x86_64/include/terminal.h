#pragma once

#include "ctype.h"
#include "alloc.h"
#include "limine.h"

#define TERMINAL_EMBEDDED_FONT
#include "os_terminal.h"

int terminal_flush_service(void *pVoid);
void init_terminal();
void update_terminal();
void terminal_putc(char c);
void terminal_puts(const char* msg);
