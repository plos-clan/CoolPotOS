#ifndef CPOS_COMMON_H
#define CPOS_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char* strcat(char *dest, const char*src);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isupper(int c);
char *int32_to_str_dec(int32_t num, int flag, int width);
char *int64_to_str_dec(int64_t num, int flag, int width);
char *uint32_to_str_hex(uint32_t num, int flag, int width);
char *uint64_to_str_hex(uint64_t num, int flag, int width);
char *uint32_to_str_oct(uint32_t num, int flag, int width);
char *insert_str(char *buf, const char *str);
int vsprintf(char *buf, const char *fmt, va_list args);

#endif