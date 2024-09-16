#ifndef CRASHPOWEROS_CTYPE_H
#define CRASHPOWEROS_CTYPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef unsigned long int off_t;
//typedef struct __mbstate_t { unsigned __opaque1, __opaque2; } mbstate_t;

int ispunct(char ch);
char toupper(char ch);
char tolower(char ch);
int iscsymf(int ch);
int isascll(char ch);
int iscntrl(char ch);
int isxdigit(int c);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isupper(int c);
int isprint(int c);
int isgraph(int c);
int islower(int c);
int isalnum(int c);

#endif
