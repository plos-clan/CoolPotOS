#pragma once

#include "common.h"

void terminal_init(uint32_t width,uint32_t height,uint32_t *screen,void* malloc_func,void* free_func,void *seri_func);
void terminal_flush();
void terminal_write_char(char c); // 旧版os_terminal putchar
void terminal_advance_state_single(char c); // 新版 putchar
void terminal_write_bstr(char* s);
void terminal_set_auto_flush(unsigned int auto_flush);