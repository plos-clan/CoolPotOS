#include "../include/common.h"
#include "../include/console.h"
#include "../include/memory.h"

static char num_str_buf[BUF_SIZE];

int is_protected_mode_enabled() {
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x1) == 0x1;
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