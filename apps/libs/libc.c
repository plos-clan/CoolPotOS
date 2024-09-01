#include "../include/stdio.h"
#include "../include/syscall.h"
#include "../include/stdlib.h"
#include "../include/string.h"

void *malloc(size_t size){
    return syscall_malloc(size);
}

void free(void *ptr){
    syscall_free(ptr);
}

void exit(int code){
    syscall_exit(code);
}

void *realloc(void *ptr, uint32_t size) {
    void *new = malloc(size);
    if (ptr) {
        memcpy(new, ptr, *(int *)((int)ptr - 4));
        free(ptr);
    }
    return new;
}

long long atoi(const char* s){
    long long temp = 0,sign = (*s <= '9' && *s >= '0') ;
    while(*s > '9' || *s < '0')s ++ ;
    if(temp == 0 && *(s - 1) == '-')sign = -1 ;
    else sign = 1 ;
    while(*s <= '9' && *s >= '0')temp = (temp * 10) + (*s - '0') , s ++ ;
    return sign * temp ;
}

void put_char(char c){
    syscall_putchar(c);
}


int fputc(int ch, FILE *stream) {
    if (CANWRITE(stream->mode)) {
        //		printk("Current Buffer=%s\n",stream->buffer);
        if (stream->p >= stream->bufferSize) {
            //	printk("Realloc....(%d,%d)\n",stream->p,stream->bufferSize);
            stream->buffer = realloc(stream->buffer, stream->bufferSize + 100);
            stream->bufferSize += 100;
        }
        if (stream->p >= stream->fileSize) {
            stream->fileSize++;
        }
        //		printk("Current Buffer=%s(A)\n",stream->buffer);
        stream->buffer[stream->p++] = ch;
        //	printk("Current Buffer=%s(B)\n",stream->buffer);
        return ch;
    }
    return EOF;
}

unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb,
                    FILE *stream) {
    if (CANWRITE(stream->mode)) {
        unsigned char *c_ptr = (unsigned char *)ptr;
        for (int i = 0; i < size * nmemb; i++) {
            fputc(c_ptr[i], stream);
        }
        return nmemb;
    } else {
        return 0;
    }
}

FILE *fopen(char *filename, char *mode) {
    unsigned int flag = 0;
    FILE *fp = (FILE *)malloc(sizeof(FILE));
    while (*mode != '\0') {
        switch (*mode) {
            case 'a':
                flag |= APPEND;
                break;
            case 'b':
                break;
            case 'r':
                flag |= READ;
                break;
            case 'w':
                flag |= WRITE;
                break;
            case '+':
                flag |= PLUS;
                break;
            default:
                break;
        }
        mode++;
    }
    if (syscall_vfs_filesize(filename) == -1) {
        free(fp);
        return NULL; // 找不到
    } else if (flag & WRITE) {
        char buffe2[100];
    }
    if (flag & WRITE) {
        fp->fileSize = 0;
    } else {
        fp->fileSize = syscall_vfs_filesize(filename);
    }
    fp->bufferSize = 0;
    if (flag & READ || flag & PLUS || flag & APPEND) {
        fp->bufferSize = syscall_vfs_filesize(filename);
    }
    if (flag & WRITE || flag & PLUS || flag & APPEND) {
        fp->bufferSize += 100;
    }
    if (fp->bufferSize == 0) {
        fp->bufferSize = 1;
    }
    fp->buffer = malloc(fp->bufferSize);
    if (flag & PLUS || flag & APPEND || flag & READ) {
        //	printk("ReadFile........\n");
        syscall_vfs_readfile(filename, fp->buffer);
    }
    fp->p = 0;
    if (flag & APPEND) {
        fp->p = fp->fileSize;
    }
    fp->name = malloc(strlen(filename) + 1);
    strcpy(fp->name, filename);
    fp->mode = flag;
    return fp;
}

int fgetc(FILE *stream) {
    if (CANREAD(stream->mode)) {
        if (stream->p >= stream->fileSize) {
            return EOF;
        } else {
            return stream->buffer[stream->p++];
        }
    } else {
        return EOF;
    }
}

unsigned int fread(void *buffer, unsigned int size, unsigned int count,
                   FILE *stream) {
    if (CANREAD(stream->mode)) {
        unsigned char *c_ptr = (unsigned char *)buffer;
        for (int i = 0; i < size * count; i++) {
            unsigned int ch = fgetc(stream);
            if (ch == EOF) {
                return i;
            } else {
                c_ptr[i] = ch;
            }
        }
        return count;
    } else {
        return 0;
    }
}

int fclose(FILE *fp) {
    if (fp == NULL) {
        return EOF;
    }
    if (CANWRITE(fp->mode)) {
        syscall_vfs_writefile(fp->name, fp->buffer, fp->fileSize);
    }
    free(fp->buffer);
    free(fp->name);
    free(fp);
    return 0;
}

char *fgets(char *str, int n, FILE *stream) {
    if (CANREAD(stream->mode)) {
        for (int i = 0; i < n; i++) {
            unsigned int ch = fgetc(stream);
            if (ch == EOF) {
                if (i == 0) {
                    return NULL;
                } else {
                    break;
                }
            }
            if (ch == '\n') {
                break;
            }
            str[i] = ch;
        }
        return str;
    }
    return NULL;
}

int fputs(const char *str, FILE *stream) {
    if (CANWRITE(stream->mode)) {
        for (int i = 0; i < strlen(str); i++) {
            fputc(str[i], stream);
        }
        return 0;
    }
    return EOF;
}

int fprintf(FILE *stream, const char *format, ...) {
    if (CANWRITE(stream->mode)) {
        int len;
        va_list ap;
        va_start(ap, format);
        char *buf = malloc(1024);
        len = vsprintf(buf, format, ap);
        fputs(buf, stream);
        free(buf);
        va_end(ap);
        return len;
    } else {
        return EOF;
    }
}

void _start(){
    extern int main(int argc,char** argv);
    int ret = main(0,NULL);
    exit(ret);
    while (1);
}