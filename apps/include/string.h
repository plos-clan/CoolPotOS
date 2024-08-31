#ifndef CRASHPOWEROS_STRING_H
#define CRASHPOWEROS_STRING_H

#include <stddef.h>

int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isupper(int c);
size_t strnlen(const char *s, size_t maxlen);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);

#endif
