#pragma once

#include "types/stdbool.h"
#include "types/stdint.h"
#include "types/stddef.h"

void   *memset(void *dest, int c, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
void   *memchr(const void *src, int c, size_t n);
size_t  strnlen(const char *str, size_t maxlen);
size_t  strlen(const char *s);
char   *strcat(char *dest, const char *src);
char   *strchrnul(const char *s, int c);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strchr(const char *s, int c);
char   *strcpy(char *dest, const char *src);
int     strcmp(const char *s1, const char *s2);
char   *strtok(char *str, const char *delim);
char   *strdup(const char *str);
char   *strndup(const char *s, size_t n);
char   *strrchr(const char *s, int c);
char   *strncpy(char *dest, const char *src, size_t n);
int64_t strtol(const char *str, char **endptr, int base);
int     memcmp(const void *a_, const void *b_, size_t size);
void   *memcpy(void *dest, const void *src, size_t n);
void    printk(const char *fmt, ...);

void *malloc(size_t size);
void free(void *ptr);
