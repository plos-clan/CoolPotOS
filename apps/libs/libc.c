#include "../include/stdio.h"
#include "../include/syscall.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/errno.h"
#include "../include/locale.h"
#include "../include/time.h"
#include "../include/math.h"
#include "../include/rand.h"

#define CVT_BUFSZ (309 + 43)

enum ranks {
    rank_char = -2,
    rank_short = -1,
    rank_int = 0,
    rank_long = 1,
    rank_longlong = 2,
};

#define MIN_RANK rank_char
#define MAX_RANK rank_longlong
#define INTMAX_RANK rank_longlong
#define SIZE_T_RANK rank_long
#define PTRDIFF_T_RANK rank_long

#define EMIT(x)                                                                \
  ({                                                                           \
    if (o < n) {                                                               \
      *q++ = (x);                                                              \
    }                                                                          \
    o++;                                                                       \
  })

int errno = 0;

enum flags {
    FL_ZERO = 0x01,   /* Zero modifier */
    FL_MINUS = 0x02,  /* Minus modifier */
    FL_PLUS = 0x04,   /* Plus modifier */
    FL_TICK = 0x08,   /* ' modifier */
    FL_SPACE = 0x10,  /* Space modifier */
    FL_HASH = 0x20,   /* # modifier */
    FL_SIGNED = 0x40, /* Number is signed */
    FL_UPPER = 0x80,  /* Upper case digits */
};

static char *aday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char *day[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                      "Thursday", "Friday", "Saturday"};

static char *amonth[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static char *month[] = {"January", "February", "March", "April",
                        "May", "June", "July", "August",
                        "September", "October", "November", "December"};

static char buf[26];

static int powers[5] = {1, 10, 100, 1000, 10000};

static char *cvt(double arg, int ndigits, int *decpt, int *sign, char *buf,
                 int eflag) {
    int r2;
    double fi, fj;
    char *p, *p1;

    if (ndigits < 0)
        ndigits = 0;
    if (ndigits >= CVT_BUFSZ - 1)
        ndigits = CVT_BUFSZ - 2;

    r2 = 0;
    *sign = 0;
    p = &buf[0];

    if (arg < 0) {
        *sign = 1;
        arg = -arg;
    }
    arg = modf(arg, &fi);
    p1 = &buf[CVT_BUFSZ];

    if (fi != 0) {
        p1 = &buf[CVT_BUFSZ];
        while (fi != 0) {
            fj = modf(fi / 10, &fi);
            *--p1 = (int) ((fj + .03) * 10) + '0';
            r2++;
        }
        while (p1 < &buf[CVT_BUFSZ])
            *p++ = *p1++;
    } else if (arg > 0) {
        while ((fj = arg * 10) < 1) {
            arg = fj;
            r2--;
        }
    }

    p1 = &buf[ndigits];
    if (eflag == 0)
        p1 += r2;
    *decpt = r2;
    if (p1 < &buf[0]) {
        buf[0] = '\0';
        return buf;
    }

    while (p <= p1 && p < &buf[CVT_BUFSZ]) {
        arg *= 10;
        arg = modf(arg, &fj);
        *p++ = (int) fj + '0';
    }

    if (p1 >= &buf[CVT_BUFSZ]) {
        buf[CVT_BUFSZ - 1] = '\0';
        return buf;
    }
    p = p1;
    *p1 += 5;

    while (*p1 > '9') {
        *p1 = '0';
        if (p1 > buf)
            ++*--p1;
        else {
            *p1 = '1';
            (*decpt)++;
            if (eflag == 0) {
                if (p > buf)
                    *p = '0';
                p++;
            }
        }
    }

    *p = '\0';
    return buf;
}

static void cfltcvt(double value, char *buffer, char fmt, int precision) {
    int decpt, sign, exp, pos;
    char *digits = 0;
    char cvtbuf[CVT_BUFSZ];
    int capexp = 0;
    int magnitude;

    if (fmt == 'G' || fmt == 'E') {
        capexp = 1;
        fmt += 'a' - 'A';
    }

    if (fmt == 'g') {
        digits = cvt(value, precision, &decpt, &sign, cvtbuf, 1);

        magnitude = decpt - 1;
        if (magnitude < -4 || magnitude > precision - 1) {
            fmt = 'e';
            precision -= 1;
        } else {
            fmt = 'f';
            precision -= decpt;
        }
    }

    if (fmt == 'e') {
        digits = cvt(value, precision + 1, &decpt, &sign, cvtbuf, 1);

        if (sign)
            *buffer++ = '-';
        *buffer++ = *digits;
        if (precision > 0)
            *buffer++ = '.';
        memcpy(buffer, digits + 1, precision);
        buffer += precision;
        *buffer++ = capexp ? 'E' : 'e';

        if (decpt == 0) {
            if (value == 0.0)
                exp = 0;
            else
                exp = -1;
        } else
            exp = decpt - 1;

        if (exp < 0) {
            *buffer++ = '-';
            exp = -exp;
        } else
            *buffer++ = '+';

        buffer[2] = (exp % 10) + '0';
        exp = exp / 10;
        buffer[1] = (exp % 10) + '0';
        exp = exp / 10;
        buffer[0] = (exp % 10) + '0';
        buffer += 3;
    } else if (fmt == 'f') {
        digits = cvt(value, precision, &decpt, &sign, cvtbuf, 0);

        if (sign)
            *buffer++ = '-';
        if (*digits) {
            if (decpt <= 0) {
                *buffer++ = '0';
                *buffer++ = '.';
                for (pos = 0; pos < -decpt; pos++)
                    *buffer++ = '0';
                while (*digits)
                    *buffer++ = *digits++;
            } else {
                pos = 0;
                while (*digits) {
                    if (pos++ == decpt)
                        *buffer++ = '.';
                    *buffer++ = *digits++;
                }
            }
        } else {
            *buffer++ = '0';
            if (precision > 0) {
                *buffer++ = '.';
                for (pos = 0; pos < precision; pos++)
                    *buffer++ = '0';
            }
        }
    }

    *buffer = '\0';
}

static size_t format_int(char *q, size_t n, uintmax_t val, enum flags flags,
                         int base, int width, int prec) {
    char *qq;
    size_t o = 0, oo;
    static const char lcdigits[] = "0123456789abcdef";
    static const char ucdigits[] = "0123456789ABCDEF";
    const char *digits;
    uintmax_t tmpval;
    int minus = 0;
    int ndigits = 0, nchars;
    int tickskip, b4tick;

    /*
     * Select type of digits
     */
    digits = (flags & FL_UPPER) ? ucdigits : lcdigits;

    /*
     * If signed, separate out the minus
     */
    if ((flags & FL_SIGNED) && ((intmax_t) val < 0)) {
        minus = 1;
        val = (uintmax_t)(-(intmax_t) val);
    }

    /*
     * Count the number of digits needed.  This returns zero for 0
     */
    tmpval = val;
    while (tmpval) {
        tmpval /= base;
        ndigits++;
    }

    /*
     * Adjust ndigits for size of output
     */
    if ((flags & FL_HASH) && (base == 8)) {
        if (prec < ndigits + 1)
            prec = ndigits + 1;
    }

    if (ndigits < prec) {
        ndigits = prec; /* Mandatory number padding */
    } else if (val == 0) {
        ndigits = 1; /* Zero still requires space */
    }

    /*
     * For ', figure out what the skip should be
     */
    if (flags & FL_TICK) {
        tickskip = (base == 16) ? 4 : 3;
    } else {
        tickskip = ndigits; /* No tick marks */
    }

    /*
     * Tick marks aren't digits, but generated by the number converter
     */
    ndigits += (ndigits - 1) / tickskip;

    /*
     * Now compute the number of nondigits
     */
    nchars = ndigits;

    if (minus || (flags & (FL_PLUS | FL_SPACE)))
        nchars++; /* Need space for sign */
    if ((flags & FL_HASH) && (base == 16)) {
        nchars += 2; /* Add 0x for hex */
    }

    /*
     * Emit early space padding
     */
    if (!(flags & (FL_MINUS | FL_ZERO)) && (width > nchars)) {
        while (width > nchars) {
            EMIT(' ');
            width--;
        }
    }

    /*
     * Emit nondigits
     */
    if (minus)
        EMIT('-');
    else if (flags & FL_PLUS)
        EMIT('+');
    else if (flags & FL_SPACE)
        EMIT(' ');

    if ((flags & FL_HASH) && (base == 16)) {
        EMIT('0');
        EMIT((flags & FL_UPPER) ? 'X' : 'x');
    }

    /*
     * Emit zero padding
     */
    if (((flags & (FL_MINUS | FL_ZERO)) == FL_ZERO) && (width > ndigits)) {
        while (width > nchars) {
            EMIT('0');
            width--;
        }
    }

    /*
     * Generate the number.  This is done from right to left
     */
    q += ndigits; /* Advance the pointer to end of number */
    o += ndigits;
    qq = q;
    oo = o; /* Temporary values */

    b4tick = tickskip;
    while (ndigits > 0) {
        if (!b4tick--) {
            qq--;
            oo--;
            ndigits--;
            if (oo < n)
                *qq = '_';
            b4tick = tickskip - 1;
        }
        qq--;
        oo--;
        ndigits--;
        if (oo < n)
            *qq = digits[val % base];
        val /= base;
    }

    /*
     * Emit late space padding
     */
    while ((flags & FL_MINUS) && (width > nchars)) {
        EMIT(' ');
        width--;
    }

    return o;
}

static void forcdecpt(char *buffer) {
    while (*buffer) {
        if (*buffer == '.')
            return;
        if (*buffer == 'e' || *buffer == 'E')
            break;
        buffer++;
    }

    if (*buffer) {
        int n = strlen(buffer);
        while (n > 0) {
            buffer[n + 1] = buffer[n];
            n--;
        }

        *buffer = '.';
    } else {
        *buffer++ = '.';
        *buffer = '\0';
    }
}

static void cropzeros(char *buffer) {
    char *stop;

    while (*buffer && *buffer != '.')
        buffer++;
    if (*buffer++) {
        while (*buffer && *buffer != 'e' && *buffer != 'E')
            buffer++;
        stop = buffer--;
        while (*buffer == '0')
            buffer--;
        if (*buffer == '.')
            buffer--;
        while ((*++buffer = *stop++));
    }
}

static void strfmt(char *str, const char *fmt, ...) {
    int ival, ilen;
    char *sval;
    va_list vp;

    va_start(vp, fmt);
    while (*fmt) {
        if (*fmt++ == '%') {
            ilen = *fmt++ - '0';
            if (ilen == 0) {
                sval = va_arg(vp,
                char *);
                while (*sval)
                    *str++ = *sval++;
            } else {
                ival = va_arg(vp,
                int);

                while (ilen) {
                    ival %= powers[ilen--];
                    *str++ = (char) ('0' + ival / powers[ilen]);
                }
            }
        } else
            *str++ = fmt[-1];
    }
    *str = '\0';
    va_end(vp);
}

static const struct lconv posix_lconv = {
        .decimal_point = ".",
        .thousands_sep = "",
        .grouping = "",
        .int_curr_symbol = "",
        .currency_symbol = "",
        .mon_decimal_point = "",
        .mon_thousands_sep = "",
        .mon_grouping = "",
        .positive_sign = "",
        .negative_sign = "",
        .int_frac_digits = -1,
        .frac_digits = -1,
        .p_cs_precedes = -1,
        .p_sep_by_space = -1,
        .n_cs_precedes = -1,
        .n_sep_by_space = -1,
        .p_sign_posn = -1,
        .n_sign_posn = -1,
        .int_p_cs_precedes = -1,
        .int_p_sep_by_space = -1,
        .int_n_cs_precedes = -1,
        .int_n_sep_by_space = -1,
        .int_p_sign_posn = -1,
        .int_n_sign_posn = -1,
};

struct lconv *localeconv(void) { return (struct lconv *) &posix_lconv; }

static int __getc() {
    char c;
    do {
        c = syscall_getch();
        if (c == '\b' || c == '\n') break;
    } while (!isprint(c));
    return c;
}

void exit(int code) {
    syscall_exit(code);
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}

long long atoi(const char *s) {
    long long temp = 0, sign = (*s <= '9' && *s >= '0');
    while (*s > '9' || *s < '0')s++;
    if (temp == 0 && *(s - 1) == '-')sign = -1;
    else sign = 1;
    while (*s <= '9' && *s >= '0')temp = (temp * 10) + (*s - '0'), s++;
    return sign * temp;
}

void put_char(char c) {
    syscall_putchar(c);
}

int filesize(const char *filename) {
    return syscall_vfs_filesize(filename);
}

int fputc(int ch, FILE *stream) {
    if (CANWRITE(stream->mode)) {

        if (strcmp("<stdout>", stream->name) || strcmp("<stderr>", stream->name)) {
            syscall_putchar(ch);
            return 0;
        }

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
    if (strcmp("<stdout>", stream->name) || strcmp("<stderr>", stream->name)) {
        syscall_print(ptr);
        return 0;
    }

    if (CANWRITE(stream->mode)) {
        unsigned char *c_ptr = (unsigned char *) ptr;
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
    FILE *fp = (FILE *) malloc(sizeof(FILE));
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
        if (!strcmp("<stdin>", stream->name)) {
            if (stream->_un_flags) {
                stream->_un_flags = false;
                return stream->buffer[stream->p++];
            }
            return __getc();
        }
        stream->_un_flags = false;
        if (stream->p >= stream->fileSize) {
            return EOF;
        } else {
            return stream->buffer[stream->p++];
        }
    } else {
        stream->_un_flags = false;
        return EOF;
    }
}

unsigned int fread(void *buffer, unsigned int size, unsigned int count,
                   FILE *stream) {
    if (CANREAD(stream->mode)) {
        unsigned char *c_ptr = (unsigned char *) buffer;
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

int fflush(FILE *stream) { return 0; }

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

        if (strcmp("<stdout>", stream->name) || strcmp("<stderr>", stream->name)) {
            syscall_print(str);
            return 0;
        }

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
        if (strcmp("<stdout>", stream->name) || strcmp("<stderr>", stream->name)) {
            syscall_print(buf);
        } else fputs(buf, stream);
        free(buf);
        va_end(ap);
        return len;
    } else {
        return EOF;
    }
}

int fseek(FILE *fp, int offset, int whence) {
    if (whence == 0) {
        fp->p = offset;
    } else if (whence == 1) {
        fp->p += offset;
    } else if (whence == 2) {
        fp->p = fp->fileSize + offset;
    } else {
        return -1;
    }
    return 0;
}

long clock() {
    return syscall_clock();
}

int feof(FILE *stream) { return fputc(' ', stream) == EOF ? -1 : 0; }

int ferror(FILE *stream) { return 0; }

long ftell(FILE *stream) { return stream->p; }

char *strerror(int errno) {
    if (errno == ENOENT) {
        return "No such file.";
    } else if (errno = EFAULT) {
        return "Bad Address";
    }
    return "(null)";
}

void ungetc(char c, FILE *stream) {
    stream->_un_flags = true;
    if (stream->p >= stream->bufferSize) {
        stream->bufferSize++;
    }
    stream->buffer[stream->p] = c;
    stream->p--;
}

int getc() {
    return fgetc(stdin);
}

int getch() {
    return syscall_getch();
}

int snprintf(char *s, size_t n, const char *fmt, ...) {
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return ret;
}

void abort() {
    exit(-1);
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *t) {
    int w, d;
    char *p, *q, *r;

    p = s;
    q = s + max - 1;
    while ((*fmt != '\0')) {
        if (*fmt++ == '%') {
            r = buf;
            switch (*fmt++) {
                case '%':
                    r = "%";
                    break;

                case 'a':
                    r = aday[t->tm_wday];
                    break;

                case 'A':
                    r = day[t->tm_wday];
                    break;

                case 'b':
                    r = amonth[t->tm_mon];
                    break;

                case 'B':
                    r = month[t->tm_mon];
                    break;

                case 'c':
                    strfmt(r, "%0 %0 %2 %2:%2:%2 %4", aday[t->tm_wday], amonth[t->tm_mon],
                           t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_year + 1970);
                    break;

                case 'd':
                    strfmt(r, "%2", t->tm_mday);
                    break;

                case 'H':
                    strfmt(r, "%2", t->tm_hour);
                    break;

                case 'I':
                    strfmt(r, "%2", (t->tm_hour % 12) ? t->tm_hour % 12 : 12);
                    break;

                case 'j':
                    strfmt(r, "%3", t->tm_yday + 1);
                    break;

                case 'm':
                    strfmt(r, "%2", t->tm_mon + 1);
                    break;

                case 'M':
                    strfmt(r, "%2", t->tm_min);
                    break;

                case 'p':
                    r = (t->tm_hour > 11) ? "PM" : "AM";
                    break;

                case 'S':
                    strfmt(r, "%2", t->tm_sec);
                    break;

                case 'U':
                    w = t->tm_yday / 7;
                    if (t->tm_yday % 7 > t->tm_wday)
                        w++;
                    strfmt(r, "%2", w);
                    break;

                case 'W':
                    w = t->tm_yday / 7;
                    if (t->tm_yday % 7 > (t->tm_wday + 6) % 7)
                        w++;
                    strfmt(r, "%2", w);
                    break;

                case 'V':
                    w = (t->tm_yday + 7 - (t->tm_wday ? t->tm_wday - 1 : 6)) / 7;
                    d = (t->tm_yday + 7 - (t->tm_wday ? t->tm_wday - 1 : 6)) % 7;

                    if (d >= 4) {
                        w++;
                    } else if (w == 0) {
                        w = 53;
                    }
                    strfmt(r, "%2", w);
                    break;

                case 'w':
                    strfmt(r, "%1", t->tm_wday);
                    break;

                case 'x':
                    strfmt(r, "%3s %3s %2 %4", aday[t->tm_wday], amonth[t->tm_mon],
                           t->tm_mday, t->tm_year + 1970);
                    break;

                case 'X':
                    strfmt(r, "%2:%2:%2", t->tm_hour, t->tm_min, t->tm_sec);
                    break;

                case 'y':
                    strfmt(r, "%2", t->tm_year % 100);
                    break;

                case 'Y':
                    strfmt(r, "%4", t->tm_year + 1970);
                    break;

                case 'Z':
                    r = t->tm_isdst ? "DST" : "GMT";
                    break;

                default:
                    buf[0] = '%';
                    buf[1] = fmt[-1];
                    buf[2] = '\0';
                    if (buf[1] == 0)
                        fmt--;
                    break;
            }
            while (*r) {
                if (p == q) {
                    *q = '\0';
                    return 0;
                }
                *p++ = *r++;
            }
        } else {
            if (p == q) {
                *q = '\0';
                return 0;
            }
            *p++ = fmt[-1];
        }
    }

    *p = '\0';
    return p - s;
}

static inline int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < 50) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next) {
        while (*next == token) *next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if (*next) *next++ = 0;
        if (argc > 50) return -1;
        argc++;
    }

    return argc;
}

void (*signal(int sig, void (*func)(int)))(int) {
    //TODO signal impl
}

void print(const char *msg) {
    syscall_print(msg);
}

void goto_xy(short x, short y) {
    //TODO gotoxy
}

void api_ReadFile(char *filename, char *buffer) {
    syscall_vfs_readfile(filename, buffer);
}

void fork() {
    //TODO fork
}

int system(char *command) {
    char *argv[50];
    cmd_parse(command, argv, ' ');
    return syscall_exec(argv[0], command, false) == 0 ? -1 : 0;
}

void sleep(int timer) {
    syscall_sleep(timer);
}

static size_t format_float(char *q, size_t n, double val, enum flags flags,
                           char fmt, int width, int prec) {
    size_t o = 0;
    char tmp[CVT_BUFSZ];
    char c, sign;
    int len, i;

    if (flags & FL_MINUS)
        flags &= ~FL_ZERO;

    c = (flags & FL_ZERO) ? '0' : ' ';
    sign = 0;
    if (flags & FL_SIGNED) {
        if (val < 0.0) {
            sign = '-';
            val = -val;
            width--;
        } else if (flags & FL_PLUS) {
            sign = '+';
            width--;
        } else if (flags & FL_SPACE) {
            sign = ' ';
            width--;
        }
    }

    if (prec < 0)
        prec = 6;
    else if (prec == 0 && fmt == 'g')
        prec = 1;

    cfltcvt(val, tmp, fmt, prec);

    if ((flags & FL_HASH) && prec == 0)
        forcdecpt(tmp);

    if (fmt == 'g' && !(flags & FL_HASH))
        cropzeros(tmp);

    len = strlen(tmp);
    width -= len;

    if (!(flags & (FL_ZERO | FL_MINUS)))
        while (width-- > 0)
            EMIT(' ');

    if (sign)
        EMIT(sign);

    if (!(flags & FL_MINUS)) {
        while (width-- > 0)
            EMIT(c);
    }

    for (i = 0; i < len; i++)
        EMIT(tmp[i]);

    while (width-- > 0)
        EMIT(' ');

    return o;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    const char *p = fmt;
    char ch;
    char *q = buf;
    size_t o = 0; /* Number of characters output */
    uintmax_t val = 0;
    int rank = rank_int; /* Default rank */
    int width = 0;
    int prec = -1;
    int base;
    size_t sz;
    enum flags flags = 0;
    enum {
        st_normal,    /* Ground state */
        st_flags,     /* Special flags */
        st_width,     /* Field width */
        st_prec,      /* Field precision */
        st_modifiers, /* Length or conversion modifiers */
    } state = st_normal;
    const char *sarg; /* %s string argument */
    char carg;        /* %c char argument */
    int slen;         /* String length */

    while ((ch = *p++)) {
        switch (state) {
            case st_normal:
                if (ch == '%') {
                    state = st_flags;
                    flags = 0;
                    rank = rank_int;
                    width = 0;
                    prec = -1;
                } else {
                    EMIT(ch);
                }
                break;

            case st_flags:
                switch (ch) {
                    case '-':
                        flags |= FL_MINUS;
                        break;
                    case '+':
                        flags |= FL_PLUS;
                        break;
                    case '\'':
                        flags |= FL_TICK;
                        break;
                    case ' ':
                        flags |= FL_SPACE;
                        break;
                    case '#':
                        flags |= FL_HASH;
                        break;
                    case '0':
                        flags |= FL_ZERO;
                        break;
                    default:
                        state = st_width;
                        p--; /* Process this character again */
                        break;
                }
                break;

            case st_width:
                if (ch >= '0' && ch <= '9') {
                    width = width * 10 + (ch - '0');
                } else if (ch == '*') {
                    width = va_arg(ap,
                    int);
                    if (width < 0) {
                        width = -width;
                        flags |= FL_MINUS;
                    }
                } else if (ch == '.') {
                    prec = 0; /* Precision given */
                    state = st_prec;
                } else {
                    state = st_modifiers;
                    p--; /* Process this character again */
                }
                break;

            case st_prec:
                if (ch >= '0' && ch <= '9') {
                    prec = prec * 10 + (ch - '0');
                } else if (ch == '*') {
                    prec = va_arg(ap,
                    int);
                    if (prec < 0)
                        prec = -1;
                } else {
                    state = st_modifiers;
                    p--; /* Process this character again */
                }
                break;

            case st_modifiers:
                switch (ch) {
                    /*
                     * Length modifiers - nonterminal sequences
                     */
                    case 'h':
                        rank--; /* Shorter rank */
                        break;
                    case 'l':
                        rank++; /* Longer rank */
                        break;
                    case 'j':
                        rank = INTMAX_RANK;
                        break;
                    case 'z':
                        rank = SIZE_T_RANK;
                        break;
                    case 't':
                        rank = PTRDIFF_T_RANK;
                        break;
                    case 'L':
                    case 'q':
                        rank += 2;
                        break;
                    default:
                        /*
                         * Next state will be normal
                         */
                        state = st_normal;

                        /*
                         * Canonicalize rank
                         */
                        if (rank < MIN_RANK)
                            rank = MIN_RANK;
                        else if (rank > MAX_RANK)
                            rank = MAX_RANK;

                        switch (ch) {
                            case 'P': /* Upper case pointer */
                                flags |= FL_UPPER;
                                break;
                            case 'p': /* Pointer */
                                base = 16;
                                prec = (8 * sizeof(void *) + 3) / 4;
                                flags |= FL_HASH;
                                val = (uintmax_t)(uintptr_t)
                                va_arg(ap,
                                void *);
                                goto is_integer;

                            case 'd': /* Signed decimal output */
                            case 'i':
                                base = 10;
                                flags |= FL_SIGNED;
                                switch (rank) {
                                    case rank_char:
                                        /* Yes, all these casts are needed */
                                        val = (uintmax_t)(intmax_t)(
                                        signed char)va_arg(ap,
                                        signed int);
                                        break;
                                    case rank_short:
                                        val = (uintmax_t)(intmax_t)(
                                        signed short)va_arg(ap,
                                        signed int);
                                        break;
                                    case rank_int:
                                        val = (uintmax_t)(intmax_t)
                                        va_arg(ap,
                                        signed int);
                                        break;
                                    case rank_long:
                                        val = (uintmax_t)(intmax_t)
                                        va_arg(ap,
                                        signed long);
                                        break;
                                    case rank_longlong:
                                        val = (uintmax_t)(intmax_t)
                                        va_arg(ap,
                                        signed long long);
                                        break;
                                }
                                goto is_integer;
                            case 'o': /* Octal */
                                base = 8;
                                goto is_unsigned;
                            case 'u': /* Unsigned decimal */
                                base = 10;
                                goto is_unsigned;
                            case 'X': /* Upper case hexadecimal */
                                flags |= FL_UPPER;
                                base = 16;
                                goto is_unsigned;
                            case 'x': /* Hexadecimal */
                                base = 16;
                                goto is_unsigned;

                            is_unsigned:
                                switch (rank) {
                                    case rank_char:
                                        val = (uintmax_t)(
                                        unsigned char)va_arg(ap,
                                        unsigned int);
                                        break;
                                    case rank_short:
                                        val = (uintmax_t)(
                                        unsigned short)va_arg(ap,
                                        unsigned int);
                                        break;
                                    case rank_int:
                                        val = (uintmax_t) va_arg(ap,
                                        unsigned int);
                                        break;
                                    case rank_long:
                                        val = (uintmax_t) va_arg(ap,
                                        unsigned long);
                                        break;
                                    case rank_longlong:
                                        val = (uintmax_t) va_arg(ap,
                                        unsigned long long);
                                        break;
                                }

                            is_integer:
                                sz =
                                        format_int(q, (o < n) ? n - o : 0, val, flags, base, width, prec);
                                q += sz;
                                o += sz;
                                break;

                            case 'c': /* Character */
                                carg = (char) va_arg(ap,
                                int);
                                sarg = &carg;
                                slen = 1;
                                goto is_string;
                            case 's': /* String */
                                sarg = va_arg(ap,
                                const char *);
                                sarg = sarg ? sarg : "(null)";
                                slen = strlen(sarg);
                                goto is_string;

                            is_string:
                            {
                                char sch;
                                int i;

                                if (prec != -1 && slen > prec)
                                    slen = prec;

                                if (width > slen && !(flags & FL_MINUS)) {
                                    char pad = (flags & FL_ZERO) ? '0' : ' ';
                                    while (width > slen) {
                                        EMIT(pad);
                                        width--;
                                    }
                                }
                                for (i = slen; i; i--) {
                                    sch = *sarg++;
                                    EMIT(sch);
                                }
                                if (width > slen && (flags & FL_MINUS)) {
                                    while (width > slen) {
                                        EMIT(' ');
                                        width--;
                                    }
                                }
                            }
                                break;

                            case 'n': {
                                /*
                                 * Output the number of characters written
                                 */
                                switch (rank) {
                                    case rank_char:
                                        *va_arg(ap,
                                        signed char *) = o;
                                        break;
                                    case rank_short:
                                        *va_arg(ap,
                                        signed short *) = o;
                                        break;
                                    case rank_int:
                                        *va_arg(ap,
                                        signed int *) = o;
                                        break;
                                    case rank_long:
                                        *va_arg(ap,
                                        signed long *) = o;
                                        break;
                                    case rank_longlong:
                                        *va_arg(ap,
                                        signed long long *) = o;
                                        break;
                                }
                            }
                                break;

                            case 'E':
                            case 'G':
                            case 'e':
                            case 'f':
                            case 'g':
                                sz =
                                        format_float(q, (o < n) ? n - o : 0, (double) (va_arg(ap,
                                double)),
                                flags, ch, width, prec);
                                q += sz;
                                o += sz;
                                break;

                            default: /* Anything else, including % */
                                EMIT(ch);
                                break;
                        }
                        break;
                }
                break;
        }
    }

    /*
     * Null-terminate the string
     */
    if (o < n)
        *q = '\0'; /* No overflow */
    else if (n > 0)
        buf[n - 1] = '\0'; /* Overflow - terminate at end of buffer */

    return o;
}

char *tmpnam(char *str) {
    static char charset[] =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const int length = sizeof(charset) - 1;
    static const int filename_length = 8; // 临时文件名长度

    if (str == NULL) {
        return NULL;
    }

    srand(clock());

    for (int i = 0; i < filename_length; ++i) {
        str[i] = charset[rand() % length];
    }
    str[filename_length] = '\0';

    return str;
}

int remove(char *filename) {
    return syscall_vfs_remove_file(filename);
}

int rename(char *filename1, char *filename2) {
    return syscall_vfs_rename(filename1, filename2);
}

//TODO
void TaskForever() {

}

void SendMessage(int to_tid, void *data, unsigned int size) {

}

void GetMessage(void *data, int from_tid) {

}

unsigned int MessageLength(int from_tid) {

}

int RAND() {
    return rand();
}

#define TOLOWER(x) ((x) | 0x20)

static unsigned long strtoul(const char *cp, char **endp, unsigned int base) {
    unsigned long result = 0, value;

    if (!base) {
        base = 10;
        if (*cp == '0') {
            base = 8;
            cp++;
            if ((TOLOWER(*cp) == 'x') && isxdigit(cp[1])) {
                cp++;
                base = 16;
            }
        }
    } else if (base == 16) {
        if (cp[0] == '0' && TOLOWER(cp[1]) == 'x')
            cp += 2;
    }
    while (isxdigit(*cp) &&
           (value = isdigit(*cp) ? *cp - '0' : TOLOWER(*cp) - 'a' + 10) < base) {
        result = result * base + value;
        cp++;
    }
    if (endp)
        *endp = (char *) cp;
    return result;
}

long strtol(const char *cp, char **endp, unsigned int base) {
    if (*cp == '-')
        return -strtoul(cp + 1, endp, base);
    return strtoul(cp, endp, base);
}


FILE *stdout;
FILE *stdin;
FILE *stderr;

bool __libc_init_mman();

void _start() {
    __libc_init_mman();

    extern int main(int argc, char **argv);
    int volatile argc = 0;
    char *volatile argv[50];

    char *arg_raw = syscall_get_arg();
    argc = cmd_parse(arg_raw, argv, ' ');

    stdout = (FILE *) malloc(sizeof(FILE));
    stdin = (FILE *) malloc(sizeof(FILE));
    stderr = (FILE *) malloc(sizeof(FILE));

    stdout->buffer = (unsigned char *) NULL;
    stdout->mode = WRITE;
    stdout->name = "<stdout>";
    stderr->name = "<stderr>";
    stdin->name = "<stdin>";
    stderr->buffer = (unsigned char *) NULL;
    stderr->mode = WRITE;
    stdin->buffer = (unsigned char *) malloc(1024);
    stdin->fileSize = -1;
    stdin->bufferSize = 1024;
    stdin->p = 0;
    stdin->mode = READ;
    stdin->_un_flags = false;

    int ret = main(argc, argv);
    exit(ret);
    while (1);
}