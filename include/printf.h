#ifndef CRASHPOWEROS_PRINTF_H
#define CRASHPOWEROS_PRINTF_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
void printf(const char *formet, ...);
void print(char *message);

#endif //CRASHPOWEROS_PRINTF_H
