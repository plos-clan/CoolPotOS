#ifndef CPOS_COMMON_H
#define CPOS_COMMON_H

#include <stddef.h>
#include <stdint.h>

#define CPOS_VERSION "0.0.3"

typedef unsigned int   u32int;
typedef          int   s32int;
typedef unsigned short u16int;
typedef          short s16int;
typedef unsigned char  u8int;
typedef          char  s8int;

size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isupper(int c);
char* int32_to_str_dec(int32_t num, int flag, int width);
char* int64_to_str_dec(int64_t num, int flag, int width);
char* uint32_to_str_hex(uint32_t num, int flag, int width);
char* uint64_to_str_hex(uint64_t num, int flag, int width);
char* uint32_to_str_oct(uint32_t num, int flag, int width);
char* insert_str(char* buf, const char* str);

int is_protected_mode_enabled();

#endif