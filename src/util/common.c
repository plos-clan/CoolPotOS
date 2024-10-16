#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/memory.h"
#include "../include/io.h"

static char num_str_buf[BUF_SIZE];

uint32_t ALIGN_F(const uint32_t addr, const uint32_t _align) {
    return (uint32_t)((addr + _align - 1) & (~(_align - 1)));
}

char *strtok(char *str, const char *delim) {
    static char *src = NULL;
    const char *indelim = delim;
    int flag = 1, index = 0;
    char *temp = NULL;
    if (str == NULL) {
        str = src;
    }
    for (; *str; str++) {
        delim = indelim;
        for (; *delim; delim++) {
            if (*str == *delim) {
                *str = NULL;
                index = 1;
                break;
            }
        }
        if (*str != NULL && flag == 1) {
            temp = str;
            flag = 0;
        }
        if (*str != NULL && flag == 0 && index == 1) {
            src = str;
            return temp;
        }
    }
    src = str;
    return temp;
}

char *strstr(char *str1, char *str2) {
    if (str1 == 0 || str2 == 0)return 0;
    const char *temp = 0;
    const char *res = 0;
    while (*str1 != '\0') {
        temp = str1;
        res = str2;
        while (*temp == *res)++temp, ++res;
        if (*res == '\0')return str1;
        ++str1;
    }
    return 0;
}

char *strncpy(char *dest, const char *src, unsigned long long count) {
    if (dest == 0 || src == 0)return 0;
    char *ret = dest;
    while (count)*dest = *src, ++dest, ++src, --count;
    return ret;
}

char *strdup(const char *str) {
    if (str == NULL)
        return NULL;

    char *strat = (char *) str;
    int len = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *) kmalloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {}

    return ret - (len + 1);
}

void insert_char(char *str, int pos, char ch) {
    int i;
    for (i = strlen(str); i >= pos; i--) {
        str[i + 1] = str[i];
    }
    str[pos] = ch;
}

void delete_char(char *str, int pos) {
    int i;
    for (i = pos; i < strlen(str); i++) {
        str[i] = str[i + 1];
    }
}

void strtoupper(char *str) {
    while (*str != '\0') {
        if (*str >= 'a' && *str <= 'z') {
            *str -= 32;
        }
        str++;
    }
}

int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *) s1,
            *p2 = (const unsigned char *) s2;
    while (n-- > 0) {
        if (*p1 != *p2)
            return *p1 - *p2;
        if (*p1 == '\0')
            return 0;
        p1++, p2++;
    }
    return 0;
}

long int strtol(const char *str, char **endptr, int base) {
    const char *s;
    unsigned long acc;
    char c;
    unsigned long cutoff;
    int neg, any, cutlim;
    s = str;
    do {
        c = *s++;
    } while (isspace((unsigned char) c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') ||
         (s[1] >= 'A' && s[1] <= 'F') ||
         (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    cutoff = neg ? (unsigned long) -(LONG_MIN + LONG_MAX) + LONG_MAX
                 : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    for (;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (!any) {
        noconv:
    } else if (neg)
        acc = -acc;
    if (endptr != NULL)
        *endptr = (char *) (any ? s - 1 : str);
    return (acc);
}

size_t strnlen(const char *s, size_t maxlen) {
    const char *es = s;
    while (*es && maxlen) {
        es++;
        maxlen--;
    }

    return (es - s);
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
    *dest = 0;
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

void insert_str(char *str, char *insert_str, int pos) {
    for (int i = 0; i < strlen(insert_str); i++) {
        insert_char(str, pos + i, insert_str[i]);
    }
}

void assert(int b, char *message) {
    if (!b) {
        printf("[KERNEL-PANIC]: %s", message);
        while (1) io_hlt();
    }
}

void trim(char *s) {
    char *p = s;
    int len = strlen(p);
    while (isspace(p[len - 1])) p[--len] = 0;
    while (*p && isspace(*p)) ++p, --len;
    memmove(s, p, len + 1);
}

char *replaceAll(char *src, char *find, char *replaceWith) {
    //如果find或者replace为null，则返回和src一样的字符串。
    if (find == NULL || replaceWith == NULL) {
        return strdup(src);
    }
    //指向替换后的字符串的head。
    char *afterReplaceHead = NULL;
    //总是指向新字符串的结尾位置。
    char *afterReplaceIndex = NULL;
    //find字符串在src字符串中出现的次数
    int count = 0;
    int i, j, k;

    int srcLen = strlen(src);
    int findLen = strlen(find);
    int replaceWithLen = strlen(replaceWith);

    //指向src字符串的某个位置，从该位置开始复制子字符串到afterReplaceIndex，初始从src的head开始复制。
    char *srcIndex = src;
    //src字符串的某个下标，从该下标开始复制字符串到afterReplaceIndex，初始为src的第一个字符。
    int cpStrStart = 0;

    //获取find字符串在src字符串中出现的次数
    count = getFindStrCount(src, find);
    //如果没有出现，则返回和src一样的字符串。
    if (count == 0) {
        return strdup(src);
    }

    //为新字符串申请内存
    afterReplaceHead = afterReplaceIndex = (char *) kmalloc(srcLen + 1 + (replaceWithLen - findLen) * count);
    //初始化新字符串内存
    memset(afterReplaceHead, '\0', sizeof(afterReplaceHead));

    for (i = 0, j = 0, k = 0; i != srcLen; i++) {
        //如果find字符串的字符和src中字符串的字符是否相同。
        if (src[i] == find[j]) {
            //如果刚开始比较，则将i的值先赋给k保存。
            if (j == 0) {
                k = i;
            }
            //如果find字符串包含在src字符串中
            if (j == (findLen - 1)) {
                j = 0;
                //拷贝src中find字符串之前的字符串到新字符串中
                strncpy(afterReplaceIndex, srcIndex, i - findLen - cpStrStart + 1);
                //修改afterReplaceIndex
                afterReplaceIndex = afterReplaceIndex + i - findLen - cpStrStart + 1;
                //修改srcIndex
                srcIndex = srcIndex + i - findLen - cpStrStart + 1;
                //cpStrStart
                cpStrStart = i + 1;

                //拷贝replaceWith字符串到新字符串中
                strncpy(afterReplaceIndex, replaceWith, replaceWithLen);
                //修改afterReplaceIndex
                afterReplaceIndex = afterReplaceIndex + replaceWithLen;
                //修改srcIndex
                srcIndex = srcIndex + findLen;
            } else {
                j++;
            }
        } else {
            //如果find和src比较过程中出现不相等的情况，则将保存的k值还给i
            if (j != 0) {
                i = k;
            }
            j = 0;
        }
    }
    //最后将src中最后一个与find匹配的字符串后面的字符串复制到新字符串中。
    strncpy(afterReplaceIndex, srcIndex, i - cpStrStart);

    return afterReplaceHead;
}

int getFindStrCount(char *src, char *find) {
    int count = 0;
    char *position = src;
    int findLen = strlen(find);
    while ((position = strstr(position, find)) != NULL) {
        count++;
        position = position + findLen;
    }
    return count;
}
