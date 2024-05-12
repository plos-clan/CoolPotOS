#ifndef CRASHPOWEROS_FILE_H
#define CRASHPOWEROS_FILE_H

#define READ 0x2
#define WRITE 0x4
#define APPEND 0x8
#define BIN 0x0
#define PLUS 0x10
#define CANREAD(flag) ((flag)&READ || (flag)&PLUS)
#define CANWRITE(flag) ((flag)&WRITE || (flag)&PLUS || (flag)&APPEND)

#include "../include/vfs.h"

int fseek(FILE *fp, int offset, int whence);
long ftell(FILE *stream);
FILE *fopen(char *filename, char *mode);
int fgetc(FILE *stream);
int fputc(int ch, FILE *stream);
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb,
                    FILE *stream);
unsigned int fread(void *buffer, unsigned int size, unsigned int count,
                   FILE *stream);
int fclose(FILE *fp);
char *fgets(char *str, int n, FILE *stream);
int fputs(const char *str, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int feof(FILE *stream);
int ferror(FILE *stream);
int fsz(char *filename);
void EDIT_FILE(char *name, char *dest, int length, int offset);
int Copy(char *path, char *path1);

#endif
