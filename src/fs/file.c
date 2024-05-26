#include "../include/file.h"
#include "../include/vfs.h"
#include "../include/memory.h"
#include "../include/printf.h"
#include <stdarg.h>

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
    FILE *fp = (FILE *)kmalloc(sizeof(FILE));
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
    if (vfs_filesize(filename) == -1) {
        kfree(fp);

        logk("NULL FILE\n");

        return NULL; // 找不到
    } else if (flag & WRITE) {
        char buffe2[100];
    }
    if (flag & WRITE) {
        fp->fileSize = 0;
    } else {
        fp->fileSize = vfs_filesize(filename);
    }
    fp->bufferSize = 0;
    if (flag & READ || flag & PLUS || flag & APPEND) {
        fp->bufferSize = vfs_filesize(filename);
    }
    if (flag & WRITE || flag & PLUS || flag & APPEND) {
        fp->bufferSize += 100;
    }
    if (fp->bufferSize == 0) {
        fp->bufferSize = 1;
    }
    fp->buffer = kmalloc(fp->bufferSize);
    if (flag & PLUS || flag & APPEND || flag & READ) {
        //	printk("ReadFile........\n");
        vfs_readfile(filename, fp->buffer);
    }
    fp->p = 0;
    if (flag & APPEND) {
        fp->p = fp->fileSize;
    }
    fp->name = kmalloc(strlen(filename) + 1);
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
        //		printk("Save file.....(%s) Size =
        //%d\n",fp->buffer,fp->fileSize);
        //  Edit_File(fp->name, fp->buffer, fp->fileSize, 0);
        vfs_writefile(fp->name, fp->buffer, fp->fileSize);
    }
    kfree(fp->buffer);
    kfree(fp->name);
    kfree(fp);
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
        char *buf = kmalloc(1024);
        len = vsprintf(buf, format, ap);
        fputs(buf, stream);
        kfree(buf);
        va_end(ap);
        return len;
    } else {
        // printk("CAN NOT WRITE\n");
        return EOF;
    }
}
