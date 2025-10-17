#pragma once

#include "types.h"
#include "types/limits.h"
#include "metadata.h"

void arch_pause();
void arch_wait_for_interrupt();

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
int64_t strtol(const char *str, char **endptr, int base);
int     memcmp(const void *a_, const void *b_, size_t size);
void   *memcpy(void *dest, const void *src, size_t n);

int isspace(int c);
