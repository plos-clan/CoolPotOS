#ifndef CRASHPOWEROS_FILE_H
#define CRASHPOWEROS_FILE_H

#define EOF -1
#define CANREAD(flag) ((flag)&READ || (flag)&PLUS)
#define CANWRITE(flag) ((flag)&WRITE || (flag)&PLUS || (flag)&APPEND)
#define READ 0x2
#define WRITE 0x4
#define APPEND 0x8
#define BIN 0x0
#define PLUS 0x10

#include <stddef.h>
#include "vfs.h"

int fgetc(FILE *stream);
FILE *fopen(char *filename, char *mode);
unsigned int fread(void *buffer, unsigned int size, unsigned int count,
                   FILE *stream);
int fclose(FILE *fp);
char *fgets(char *str, int n, FILE *stream);
int fputs(const char *str, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int fputc(int ch, FILE *stream);
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb,
                    FILE *stream);

#endif
