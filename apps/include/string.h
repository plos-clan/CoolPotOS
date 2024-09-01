#ifndef CRASHPOWEROS_STRING_H
#define CRASHPOWEROS_STRING_H

#include "ctype.h"

const char* memchr(const char* buf,char c,unsigned long long count);
void *memmove(void *dest, const void *src, size_t num);
void *memset(void *s, int c, size_t n);
int memcmp(const void *a_, const void *b_, uint32_t size);

void* memcpy(void* s, const void* ct, size_t n);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
const char* strstr(const char *str1,const char *str2);
char *strdup(const char *str);

char* strncat(char* dest,const char* src,unsigned long long count);
size_t strnlen(const char *s, size_t maxlen);
char* strncpy(char* dest, const char* src,unsigned long long count);

#endif
