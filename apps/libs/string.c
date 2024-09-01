#include "../include/string.h"
#include "../include/ctype.h"
#include "../include/stdlib.h"

int ispunct(char ch) {
    return ch == '!' || ch == '@' || ch == '#' || ch == '$' || ch == '%' || ch == '^' || ch == '&' || ch == '*' ||
           ch == '(' || ch == ')' || ch == '+' || ch == '-' || ch == '/' || ch == '?' || ch == '"' || ch == '\'' ||
           ch == ':' || ch == ';' || ch == '{' || ch == '[' || ch == ']' || ch == '}' || ch == '|' || ch == '\\' ||
           ch == '~' || ch == '`' || ch == '<' || ch == '>' || ch == ',' || ch == '.';
}

char toupper(char ch) {
    return (ch & 0xDF);
}

char tolower(char ch) {
    return (ch | 0x20);
}

int iscsymf(int ch) {
    return ch == '_' || isdigit(ch) || isalpha(ch);
}

int isascll(char ch) {
    return ((ch & 0x80) == 0);
}

int iscntrl(char ch) {
    return ((ch == 0x7F) || (ch >= 0x00 && ch <= 0x1F));
}

int isprint(int c) {
    return (c > 0x1F && c < 0x7F);
}

int isgraph(int c) {
    return (c > 0x20 && c < 0x7F);
}

int isxdigit(int c) {
    if (!((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        if (!isdigit(c)) return 0;
    }
    return 1;
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
            c == '\v');
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

const char *memchr(const char *buf, char c, unsigned long long count) {
    while (count) {
        if (*buf == c)
            return buf;
        ++count;
        ++buf;
    }
    return 0;
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

void *memcpy(void *s, const void *ct, size_t n) {
    if (NULL == s || NULL == ct || n <= 0)
        return NULL;
    while (n--)
        *(char *) s++ = *(char *) ct++;
    return s;
}

int memcmp(const void *a_, const void *b_, uint32_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n-- > 0)
        *p++ = c;
    return s;
}

void *memmove(void *dest, const void *src, size_t num) {
    void *ret = dest;
    if (dest < src) {
        while (num--)//前->后
        {
            *(char *) dest = *(char *) src;
            dest = (char *) dest + 1;
            src = (char *) src + 1;
        }
    } else {
        while (num--)//后->前
        {
            *((char *) dest + num) = *((char *) src + num);
        }
    }
    return ret;
}

char *strncat(char *dest, const char *src, unsigned long long count) {
    if (dest == 0 || src == 0)return 0;
    char *ret = dest;
    while (*dest != '\0')dest++;
    for (unsigned long long i = 0; i < count; ++i)dest[i] = src[i];
    dest[count] = '\0';
    return ret;
}

char *strncpy(char *dest, const char *src, unsigned long long count) {
    if (dest == 0 || src == 0)return 0;
    char *ret = dest;
    while (count)*dest = *src, ++dest, ++src, --count;
    return ret;
}

const char *strstr(const char *str1, const char *str2) {
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

char *strdup(const char *str) {
    if (str == NULL)
        return NULL;

    char *strat = (char *) str;
    int len = 0;
    while (*str++ != '\0')
        len++;
    char *ret = (char *) malloc(len + 1);

    while ((*ret++ = *strat++) != '\0') {}

    return ret - (len + 1);
}
