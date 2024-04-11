#ifndef CRASHPOWEROS_COMMON_H
#define CRASHPOWEROS_COMMON_H

#define OS_NAME "CrashPowerDOS"
#define OS_VERSION "v0.2.2"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void assert(int b,char* message);
size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char* strcat(char *dest, const char*src);
void trim(char *s);
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
int sprintf(char *buf, const char *fmt, ...);

#endif //CRASHPOWEROS_COMMON_H
