#ifndef CRASHPOWEROS_STDIO_H
#define CRASHPOWEROS_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void put_char(char a);
int printf(const char* fmt, ...);
int puts(const char *s);
int vsprintf(char *buf, const char *fmt, va_list args);

#endif
