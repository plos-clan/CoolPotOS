#pragma once

#define KMSG_SIZE (1 << 17) // 内核日志环形缓冲区大小 (128k)

#include "types.h"

void kmsg_putc(char c);
void kmsg_write(const char *s);
int kmsg_getc(void);

void logk(const char *str);
void logkf(char *fmt, ...);
