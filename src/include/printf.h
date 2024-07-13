#ifndef CRASHPOWEROS_PRINTF_H
#define CRASHPOWEROS_PRINTF_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "common.h"

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
void printf(const char *formet, ...);
void print(char *message);
void printk(const char *formet, ...);
void putchar(char c);
void logk(char *message);
void logkf(char *formet,...);
void screen_clear();
void klogf(bool isok,char* fmt,...);

#endif //CRASHPOWEROS_PRINTF_H
