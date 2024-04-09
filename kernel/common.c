#include "../include/common.h"
#include "../include/vga.h"
#include "../include/memory.h"
#include "../include/io.h"

static char num_str_buf[BUF_SIZE];

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    int retval;
    va_start(args, fmt);
    retval = vsprintf(buf, fmt, args);
    va_end(args);
    return retval;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    char is_equal = 1;

    for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++) {
        if (*s1 != *s2) {
            is_equal = 0;
            break;
        }
    }

    if (is_equal) {
        if (*s1 != '\0') {
            return 1;
        } else if (*s2 != '\0') {
            return -1;
        } else {
            return 0;
        }
    } else {
        return (int) (*s1 - *s2);
    }
}

char *strcpy(char *dest, const char *src) {
    do {
        *dest++ = *src++;
    } while (*src != 0);
}

char *strcat(char *dest, const char *src) {
    char *temp = dest;
    while (*temp != '\0')
        temp++;
    while ((*temp++ = *src++) != '\0');
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
            c == '\v');
}

// isdigit
int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

// isalpha
int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// isupper
int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

char *int32_to_str_dec(int32_t num, int flag, int width) {
    int32_t num_tmp = num;
    char *p = num_str_buf;
    char *q = NULL;
    int len = 0;
    char *str_first = NULL;
    char *str_end = NULL;
    char ch = 0;

    memset(num_str_buf, 0, sizeof(num_str_buf));

    char dic[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    if (num_tmp < 0) {
        *p++ = '-';
    } else if (flag & FLAG_SIGN) {
        *p++ = '+';
    } else {
        *p++ = ' ';
    }
    str_first = num_str_buf;

    if (num_tmp >= 0 && !(flag & FLAG_SIGN)) {
        str_first++;
    }

    if (num_tmp == 0) {
        *p++ = '0';
    } else {
        if (num_tmp < 0)
            num_tmp = -num_tmp;

        while (num_tmp) {
            *p++ = dic[num_tmp % 10];
            num_tmp /= 10;
        }
    }
    *p = '\0';

    str_end = p;
    len = str_end - str_first;

    q = num_str_buf + 1;
    p--;
    while (q < p) {
        ch = *q;
        *q = *p;
        *p = ch;
        p--;
        q++;
    }

    if (len < width) {
        p = str_end;

        if (flag & FLAG_LEFT_ADJUST) {
            for (int i = 0; i < width - len; i++)
                *p++ = ' ';
            *p = '\0';
            str_end = p;
        } else {
            while (p >= str_first) {
                *(p + width - len) = *p;
                p--;
            }

            if (flag & FLAG_ZERO_PAD) {
                for (int i = 0; i < width - len; i++) {
                    num_str_buf[i + 1] = '0';
                }
            } else {
                for (int i = 0; i < width - len; i++)
                    str_first[i] = ' ';
            }
        }
    }

    return str_first;
}

char *int64_to_str_dec(int64_t num, int flag, int width) {
    return 0;
}

char *uint32_to_str_hex(uint32_t num, int flag, int width) {
    uint32_t num_tmp = num;
    char *p = num_str_buf;
    char *q = NULL;
    int len = 0;
    char *str_first = NULL;
    char *str_end = NULL;
    char ch = 0;

    memset(num_str_buf, 0, sizeof(num_str_buf));

    char dic_lower[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char dic_upper[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    char *dic = (flag & FLAG_LOWER) ? dic_lower : dic_upper;

    str_first = num_str_buf;

    *p++ = '0';
    *p++ = (flag & FLAG_LOWER) ? 'x' : 'X';

    if (!(flag & FLAG_ALTNT_FORM)) {
        str_first += 2;
    }

    do {
        *p++ = dic[num_tmp % 16];
        num_tmp /= 16;
    } while (num_tmp);
    *p = '\0';

    str_end = p;
    len = str_end - str_first;

    q = num_str_buf + 2;
    p--;
    while (q < p) {
        ch = *q;
        *q = *p;
        *p = ch;
        p--;
        q++;
    }

    if (len < width) {
        p = str_end;

        if (flag & FLAG_LEFT_ADJUST) {
            for (int i = 0; i < width - len; i++)
                *p++ = ' ';
            *p = '\0';
            str_end = p;
        } else {
            while (p >= str_first) {
                *(p + width - len) = *p;
                p--;
            }
            if (flag & FLAG_ALTNT_FORM)
                str_first[1] = (flag & FLAG_LOWER) ? 'x' : 'X';

            if (flag & FLAG_ZERO_PAD) {
                for (int i = 0; i < width - len; i++) {
                    num_str_buf[i + 2] = '0';
                }
            } else {
                for (int i = 0; i < width - len; i++)
                    str_first[i] = ' ';
            }
        }
    }

    if (num == 0 && flag & FLAG_ALTNT_FORM)
        str_first[1] = '0';

    return str_first;
}

char *uint64_to_str_hex(uint64_t num, int flag, int width) {
    uint64_t num_tmp = num;
    char *p = num_str_buf;
    char *q = NULL;
    int len = 0;
    char *str_first = NULL;
    char *str_end = NULL;
    char ch = 0;

    memset(num_str_buf, 0, sizeof(num_str_buf));

    char dic_lower[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char dic_upper[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    char *dic = (flag & FLAG_LOWER) ? dic_lower : dic_upper;

    str_first = num_str_buf;

    *p++ = '0';
    *p++ = (flag & FLAG_LOWER) ? 'x' : 'X';

    if (!(flag & FLAG_ALTNT_FORM)) {
        str_first += 2;
    }

    while (num_tmp) {
        *p++ = dic[num_tmp % 16];
        num_tmp /= 16;
    }
    *p = '\0';

    str_end = p;
    len = str_end - str_first;

    q = num_str_buf + 2;
    p--;
    while (q < p) {
        ch = *q;
        *q = *p;
        *p = ch;
        p--;
        q++;
    }

    if (len < width) {
        p = str_end;

        while (p >= str_first) {
            *(p + width - len) = *p;
            p--;
        }

        if (flag & FLAG_ZERO_PAD) {
            for (int i = 0; i < width - len; i++) {
                num_str_buf[i + 2] = '0';
            }
        } else {
            for (int i = 0; i < width - len; i++)
                str_first[i] = ' ';
        }
    }

    return str_first;
}

char *uint32_to_str_oct(uint32_t num, int flag, int width) {
    uint32_t num_tmp = num;
    char *p = num_str_buf;
    char *q = NULL;
    int len = 0;
    char *str_first = NULL;
    char *str_end = NULL;
    char ch = 0;

    memset(num_str_buf, 0, sizeof(num_str_buf));

    char dic[] = {'0', '1', '2', '3', '4', '5', '6', '7'};

    str_first = num_str_buf;

    *p++ = '0';

    if (!(flag & FLAG_ALTNT_FORM)) {
        str_first += 1;
    }

    while (num_tmp) {
        *p++ = dic[num_tmp % 8];
        num_tmp /= 8;
    }
    *p = '\0';

    str_end = p;
    len = str_end - str_first;

    q = num_str_buf + 1;
    p--;
    while (q < p) {
        ch = *q;
        *q = *p;
        *p = ch;
        p--;
        q++;
    }

    if (len < width) {
        p = str_end;

        if (flag & FLAG_LEFT_ADJUST) {
            for (int i = 0; i < width - len; i++)
                *p++ = ' ';
            *p = '\0';
            str_end = p;
        } else {
            while (p >= str_first) {
                *(p + width - len) = *p;
                p--;
            }

            if (flag & FLAG_ZERO_PAD) {
                for (int i = 0; i < width - len; i++) {
                    num_str_buf[i + 1] = '0';
                }
            } else {
                for (int i = 0; i < width - len; i++)
                    str_first[i] = ' ';
            }
        }
    }

    return str_first;
}

char *insert_str(char *buf, const char *str) {
    char *p = buf;

    while (*str) {
        *p++ = *str++;
    }

    return p;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    char *str = buf;
    int flag = 0;
    int int_type = INT_TYPE_INT;
    int tot_width = 0;
    int sub_width = 0;
    char buf2[64] = {0};
    char *s = NULL;
    char ch = 0;
    int8_t num_8 = 0;
    uint8_t num_u8 = 0;
    int16_t num_16 = 0;
    uint16_t num_u16 = 0;
    int32_t num_32 = 0;
    uint32_t num_u32 = 0;
    int64_t num_64 = 0;
    uint64_t num_u64 = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            *str++ = *p;
            continue;
        }

        flag = 0;
        tot_width = 0;
        sub_width = 0;
        int_type = INT_TYPE_INT;

        p++;

        while (*p == FLAG_ALTNT_FORM_CH || *p == FLAG_ZERO_PAD_CH ||
               *p == FLAG_LEFT_ADJUST_CH || *p == FLAG_SPACE_BEFORE_POS_NUM_CH ||
               *p == FLAG_SIGN_CH) {
            if (*p == FLAG_ALTNT_FORM_CH) {
                flag |= FLAG_ALTNT_FORM;
            } else if (*p == FLAG_ZERO_PAD_CH) {
                flag |= FLAG_ZERO_PAD;
            } else if (*p == FLAG_LEFT_ADJUST_CH) {
                flag |= FLAG_LEFT_ADJUST;
                flag &= ~FLAG_ZERO_PAD;
            } else if (*p == FLAG_SPACE_BEFORE_POS_NUM_CH) {
                flag |= FLAG_SPACE_BEFORE_POS_NUM;
            } else if (*p == FLAG_SIGN_CH) {
                flag |= FLAG_SIGN;
            } else {
            }

            p++;
        }

        if (*p == '*') {
            tot_width = va_arg(args,
            int);
            if (tot_width < 0)
                tot_width = 0;
            p++;
        } else {
            while (isdigit(*p)) {
                tot_width = tot_width * 10 + *p - '0';
                p++;
            }
        }
        if (*p == '.') {
            if (*p == '*') {
                sub_width = va_arg(args,
                int);
                if (sub_width < 0)
                    sub_width = 0;
                p++;
            } else {
                while (isdigit(*p)) {
                    sub_width = sub_width * 10 + *p - '0';
                    p++;
                }
            }
        }

        LOOP_switch:
        switch (*p) {
            case 'h':
                p++;
                if (int_type >= INT_TYPE_MIN) {
                    int_type >>= 1;
                    goto LOOP_switch;
                } else {
                    *str++ = '%';
                    break;
                }
            case 'l':
                p++;
                if (int_type <= INT_TYPE_MAX) {
                    int_type <<= 1;
                    goto LOOP_switch;
                } else {
                    *str++ = '%';
                    break;
                }
            case 's':
                s = va_arg(args,
                char *);
                str = insert_str(str, s);
                break;
            case 'c':
                ch = (char) (va_arg(args,
                int) &
                0xFF);
                *str++ = ch;
                break;
            case 'd':
                switch (int_type) {
                    case INT_TYPE_CHAR:
                        num_8 = (int8_t) va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_8, flag, tot_width));
                        break;
                    case INT_TYPE_SHORT:
                        num_16 = (int16_t) va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_16, flag, tot_width));
                        break;
                    case INT_TYPE_INT:
                        num_32 = va_arg(args, int32_t);
                        str = insert_str(str, int32_to_str_dec(num_32, flag, tot_width));
                        break;
                    case INT_TYPE_LONG:
                        num_64 = va_arg(args, int64_t);
                        str = insert_str(str, int64_to_str_dec(num_64, flag, tot_width));
                        break;
                    case INT_TYPE_LONG_LONG:
                        num_64 = va_arg(args, int64_t);
                        str = insert_str(str, int64_to_str_dec(num_64, flag, tot_width));
                        break;
                }
                break;
            case 'x':
                flag |= FLAG_LOWER;
            case 'X':
                switch (int_type) {
                    case INT_TYPE_CHAR:
                        num_u8 = (uint8_t)
                                va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u8, flag, tot_width));
                        break;
                    case INT_TYPE_SHORT:
                        num_u16 = (uint16_t)
                                va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u16, flag, tot_width));
                        break;
                    case INT_TYPE_INT:
                        num_u32 = va_arg(args, uint32_t);
                        str = insert_str(str, uint32_to_str_hex(num_u32, flag, tot_width));
                        break;
                    case INT_TYPE_LONG:
                        num_u64 = va_arg(args, uint64_t);
                        str = insert_str(str, uint64_to_str_hex(num_u64, flag, tot_width));
                        break;
                    case INT_TYPE_LONG_LONG:
                        num_u64 = va_arg(args, uint64_t);
                        str = insert_str(str, uint64_to_str_hex(num_u64, flag, tot_width));
                        break;
                }
                break;
            case 'o':
                num_u32 = va_arg(args, uint32_t);
                str = insert_str(str, uint32_to_str_oct(num_u32, flag, tot_width));
                break;
            case '%':
                *str++ = '%';
                break;
            default:
                *str++ = '%';
                *str++ = *p;
                break;
        }
    }
    *str = '\0';

    return str - buf;
}

void assert(int b,char* message){
    if(!b){
        printf("[KERNEL-PANIC]: %s",message);
        while (1) io_hlt();
    }
}

void trim(char *s){
    char *p = s;
    int len = strlen(p);
    while (isspace(p[len - 1])) p[--len] = 0;
    while (*p && isspace(*p)) ++p,--len;
    memmove(s,p,len+1);
}
