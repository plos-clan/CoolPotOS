#ifndef CRASHPOWEROS_COMMON_H
#define CRASHPOWEROS_COMMON_H

#define OS_NAME "CoolPotOS"
#define OS_VERSION "v0.2.9"

// b 0x211972
// b 0x20d0a6


#define LONG_MAX 9223372036854775807L
#define LONG_MIN -9223372036854775808L

#define UINT32_MAX 0xffffffff
#define INT32_MAX 0x7fffffff

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)

#define swap32(x)                                                              \
  ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) |                    \
   (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#define swap16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

typedef int bool;
#define true 1
#define false 0
#define EOF -1

unsigned int rand(void);
void srand(unsigned long seed);
void insert_char(char* str, int pos, char ch);
void delete_char(char* str, int pos);
void strtoupper(char* str);
int strncmp(const char* s1, const char* s2, size_t n);
void assert(int b,char* message);
size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char* strcat(char *dest, const char*src);
size_t strnlen(const char *s, size_t maxlen);
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
void insert_str(char *str, char *insert_str, int pos);
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
long int strtol(const char *str,char **endptr,int base);

void reset_kernel();
void shutdown_kernel();
uint32_t memory_all();

uint32_t ALIGN_F(const uint32_t addr, const uint32_t _align);

#endif //CRASHPOWEROS_COMMON_H
