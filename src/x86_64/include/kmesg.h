#pragma once

void kmsg_putc(char c);
void kmsg_write(const char *s);
int  kmsg_getc(void);
void build_kmesg_device();
