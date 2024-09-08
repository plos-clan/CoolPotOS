#ifndef CRASHPOWEROS_STDIO_H
#define CRASHPOWEROS_STDIO_H

#define EOF -1
#define CANREAD(flag) ((flag)&READ || (flag)&PLUS)
#define CANWRITE(flag) ((flag)&WRITE || (flag)&PLUS || (flag)&APPEND)
#define READ 0x2
#define WRITE 0x4
#define APPEND 0x8
#define BIN 0x0
#define PLUS 0x10
#define BUFSIZ (4096*2)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#include "ctype.h"

typedef struct FILE {
    unsigned int mode;
    unsigned int fileSize;
    unsigned char *buffer;
    unsigned int bufferSize;
    unsigned int p;
    char *name;
} FILE;

extern FILE *stdout;
extern FILE *stdin;
extern FILE *stderr;

int getc();
int getch();
void put_char(char a);
int scanf(const char *format, ...);
int printf(const char* fmt, ...);
void print(const char* msg);
int puts(const char *s);
int vsprintf(char *buf, const char *fmt, va_list args);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
int fgetc(FILE *stream);
FILE *fopen(char *filename, char *mode);
unsigned int fread(void *buffer, unsigned int size, unsigned int count,
                   FILE *stream);
int fclose(FILE *fp);
char *fgets(char *str, int n, FILE *stream);
int fputs(const char *str, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int fputc(int ch, FILE *stream);
int fflush(FILE *stream);
unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb,
                    FILE *stream);
int fseek(FILE *fp, int offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);

int filesize(const char* filename);
int remove(char *filename);
int rename(char *filename1, char *filename2);

#endif
