#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/ctype.h"
#include "../include/string.h"

int vfscanf(FILE *f, const char *fmt, va_list ap) {

    int count = 0, i = 0;
    char input;
    int d, sign, first, *pd;
    char *s;
    while (fmt[i]) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'c':
                    s = va_arg(ap,
                    char*);
                    input = fgetc(f);
                    if (input == EOF)
                        goto END_EOF;
                    *s = input;
                    count++;
                    break;
                case 'd':
                    pd = va_arg(ap,int*);
                    d = 0;
                    first = 1;
                    sign = 1;

                    while (1) {
                        input = fgetc(f);

                        if (input == EOF)
                            goto END_EOF;
                        else if (input >= '0' && input <= '9') {
                            d *= 10;
                            d += input - '0';
                            if (first)
                                first = 0;
                        } else {
                            if (first) {
                                if (input == '+') {
                                    sign = 1;
                                    first = 0;
                                } else if (input == '-') {
                                    sign = -1;
                                    first = 0;
                                } else
                                    goto END;
                            } else {
                                *pd = sign * d;
                                count++;
                                break;
                            }
                        }
                    }
                    break;
                case 's':
                    s = va_arg(ap,char*);
                    while (1) {
                        input = fgetc(f);
                        if (input == EOF)
                            goto END_EOF;
                        else if (!isspace(input))
                            *s++ = input;
                        else {
                            *s = '\0';
                            count++;
                            break;
                        }
                    }
                    break;
                default:
                    goto END;
            }
        } else if (isspace(fmt[i])) {
            if (input == EOF)
                goto END_EOF;
        } else {
            input = fgetc(f);
            if (input == EOF)
                goto END_EOF;
            else if (input != fmt[i]) {
                ungetc(input, stdin);
                goto END;
            }
        }
        i++;
    }

    END:
    return count;
    END_EOF:
    return EOF;
}


int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap) {
    FILE *file = (FILE *) malloc(sizeof(FILE));
    file->buffer = s;
    file->name = "<vscanf>";
    return vfscanf(file, fmt, ap);
}

int sscanf(const char *restrict s, const char *restrict fmt, ...) {
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsscanf(s, fmt, ap);
    va_end(ap);
    return ret;
}

int scanf(const char *format, ...) {
    int ret;
    va_list ap;
    va_start(ap, format);
    char buffer[100];
    ret = vfscanf(stdin, format, ap);
    va_end(ap);
    return ret;
}