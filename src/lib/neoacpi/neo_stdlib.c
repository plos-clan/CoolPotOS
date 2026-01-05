#include "lib/neoacpi/neo_stdlib.h"

enum parse_number_mode {
    PARSE_NUMBER_MODE_MAYBE,
    PARSE_NUMBER_MODE_MUST,
};

enum neo_acpi_base {
    NEO_ACPI_BASE_AUTO,
    NEO_ACPI_BASE_OCT = 8,
    NEO_ACPI_BASE_DEC = 10,
    NEO_ACPI_BASE_HEX = 16,
};

enum char_type {
    CHAR_TYPE_CONTROL     = 1 << 0,
    CHAR_TYPE_SPACE       = 1 << 1,
    CHAR_TYPE_BLANK       = 1 << 2,
    CHAR_TYPE_PUNCTUATION = 1 << 3,
    CHAR_TYPE_LOWER       = 1 << 4,
    CHAR_TYPE_UPPER       = 1 << 5,
    CHAR_TYPE_DIGIT       = 1 << 6,
    CHAR_TYPE_HEX_DIGIT   = 1 << 7,
    CHAR_TYPE_ALPHA       = CHAR_TYPE_LOWER | CHAR_TYPE_UPPER,
    CHAR_TYPE_ALHEX       = CHAR_TYPE_ALPHA | CHAR_TYPE_HEX_DIGIT,
    CHAR_TYPE_ALNUM       = CHAR_TYPE_ALPHA | CHAR_TYPE_DIGIT,
};

static const uint8_t ascii_map[256] = {
    CHAR_TYPE_CONTROL, // 0
    CHAR_TYPE_CONTROL, // 1
    CHAR_TYPE_CONTROL, // 2
    CHAR_TYPE_CONTROL, // 3
    CHAR_TYPE_CONTROL, // 4
    CHAR_TYPE_CONTROL, // 5
    CHAR_TYPE_CONTROL, // 6
    CHAR_TYPE_CONTROL, // 7
    CHAR_TYPE_CONTROL, // -> 8 control codes

    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE | CHAR_TYPE_BLANK, // 9 tab

    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 10
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 11
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 12
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // -> 13 whitespaces

    CHAR_TYPE_CONTROL, // 14
    CHAR_TYPE_CONTROL, // 15
    CHAR_TYPE_CONTROL, // 16
    CHAR_TYPE_CONTROL, // 17
    CHAR_TYPE_CONTROL, // 18
    CHAR_TYPE_CONTROL, // 19
    CHAR_TYPE_CONTROL, // 20
    CHAR_TYPE_CONTROL, // 21
    CHAR_TYPE_CONTROL, // 22
    CHAR_TYPE_CONTROL, // 23
    CHAR_TYPE_CONTROL, // 24
    CHAR_TYPE_CONTROL, // 25
    CHAR_TYPE_CONTROL, // 26
    CHAR_TYPE_CONTROL, // 27
    CHAR_TYPE_CONTROL, // 28
    CHAR_TYPE_CONTROL, // 29
    CHAR_TYPE_CONTROL, // 30
    CHAR_TYPE_CONTROL, // -> 31 control codes

    CHAR_TYPE_SPACE | CHAR_TYPE_BLANK, // 32 space

    CHAR_TYPE_PUNCTUATION, // 33
    CHAR_TYPE_PUNCTUATION, // 34
    CHAR_TYPE_PUNCTUATION, // 35
    CHAR_TYPE_PUNCTUATION, // 36
    CHAR_TYPE_PUNCTUATION, // 37
    CHAR_TYPE_PUNCTUATION, // 38
    CHAR_TYPE_PUNCTUATION, // 39
    CHAR_TYPE_PUNCTUATION, // 40
    CHAR_TYPE_PUNCTUATION, // 41
    CHAR_TYPE_PUNCTUATION, // 42
    CHAR_TYPE_PUNCTUATION, // 43
    CHAR_TYPE_PUNCTUATION, // 44
    CHAR_TYPE_PUNCTUATION, // 45
    CHAR_TYPE_PUNCTUATION, // 46
    CHAR_TYPE_PUNCTUATION, // -> 47 punctuation

    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 48
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 49
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 50
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 51
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 52
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 53
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 54
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 55
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 56
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // -> 57 digits

    CHAR_TYPE_PUNCTUATION, // 58
    CHAR_TYPE_PUNCTUATION, // 59
    CHAR_TYPE_PUNCTUATION, // 60
    CHAR_TYPE_PUNCTUATION, // 61
    CHAR_TYPE_PUNCTUATION, // 62
    CHAR_TYPE_PUNCTUATION, // 63
    CHAR_TYPE_PUNCTUATION, // -> 64 punctuation

    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 65
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 66
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 67
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 68
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 69
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // -> 70 ABCDEF

    CHAR_TYPE_UPPER, // 71
    CHAR_TYPE_UPPER, // 72
    CHAR_TYPE_UPPER, // 73
    CHAR_TYPE_UPPER, // 74
    CHAR_TYPE_UPPER, // 75
    CHAR_TYPE_UPPER, // 76
    CHAR_TYPE_UPPER, // 77
    CHAR_TYPE_UPPER, // 78
    CHAR_TYPE_UPPER, // 79
    CHAR_TYPE_UPPER, // 80
    CHAR_TYPE_UPPER, // 81
    CHAR_TYPE_UPPER, // 82
    CHAR_TYPE_UPPER, // 83
    CHAR_TYPE_UPPER, // 84
    CHAR_TYPE_UPPER, // 85
    CHAR_TYPE_UPPER, // 86
    CHAR_TYPE_UPPER, // 87
    CHAR_TYPE_UPPER, // 88
    CHAR_TYPE_UPPER, // 89
    CHAR_TYPE_UPPER, // -> 90 the rest of UPPERCASE alphabet

    CHAR_TYPE_PUNCTUATION, // 91
    CHAR_TYPE_PUNCTUATION, // 92
    CHAR_TYPE_PUNCTUATION, // 93
    CHAR_TYPE_PUNCTUATION, // 94
    CHAR_TYPE_PUNCTUATION, // 95
    CHAR_TYPE_PUNCTUATION, // -> 96 punctuation

    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 97
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 98
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 99
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 100
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 101
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // -> 102 abcdef

    CHAR_TYPE_LOWER, // 103
    CHAR_TYPE_LOWER, // 104
    CHAR_TYPE_LOWER, // 105
    CHAR_TYPE_LOWER, // 106
    CHAR_TYPE_LOWER, // 107
    CHAR_TYPE_LOWER, // 108
    CHAR_TYPE_LOWER, // 109
    CHAR_TYPE_LOWER, // 110
    CHAR_TYPE_LOWER, // 111
    CHAR_TYPE_LOWER, // 112
    CHAR_TYPE_LOWER, // 113
    CHAR_TYPE_LOWER, // 114
    CHAR_TYPE_LOWER, // 115
    CHAR_TYPE_LOWER, // 116
    CHAR_TYPE_LOWER, // 117
    CHAR_TYPE_LOWER, // 118
    CHAR_TYPE_LOWER, // 119
    CHAR_TYPE_LOWER, // 120
    CHAR_TYPE_LOWER, // 121
    CHAR_TYPE_LOWER, // -> 122 the rest of UPPERCASE alphabet

    CHAR_TYPE_PUNCTUATION, // 123
    CHAR_TYPE_PUNCTUATION, // 124
    CHAR_TYPE_PUNCTUATION, // 125
    CHAR_TYPE_PUNCTUATION, // -> 126 punctuation

    CHAR_TYPE_CONTROL // 127 backspace
};

#define WT         size_t
#define WS         (sizeof(WT))
#define SS         (sizeof(size_t))
#define ALIGN      (sizeof(size_t) - 1)
#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define BITOP(a, b, op)                                                                            \
    ((a)[(size_t)(b) / (8 * sizeof *(a))] op(size_t) 1 << ((size_t)(b) % (8 * sizeof *(a))))

int neo_acpi_memcmp(const void *a_, const void *b_, size_t size) {
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *neo_acpi_memset(void *dest, int c, size_t n) {
    unsigned char *s = dest;
    size_t         k = 0;

    if (!n) return dest;
    s[0]     = c;
    s[n - 1] = c;
    if (n <= 2) return dest;
    s[1]     = c;
    s[2]     = c;
    s[n - 2] = c;
    s[n - 3] = c;
    if (n <= 6) return dest;
    s[3]     = c;
    s[n - 4] = c;
    if (n <= 8) return dest;

    k  = -(uintptr_t)s & 3;
    s += k;
    n -= k;
    n &= -4;

#ifdef __GNUC__
    typedef uint32_t __attribute__((__may_alias__)) u32;
    typedef uint64_t __attribute__((__may_alias__)) u64;

    u32 c32 = ((u32)-1) / 255 * (unsigned char)c;

    *(u32 *)(s + 0)     = c32;
    *(u32 *)(s + n - 4) = c32;
    if (n <= 8) return dest;
    *(u32 *)(s + 4)      = c32;
    *(u32 *)(s + 8)      = c32;
    *(u32 *)(s + n - 12) = c32;
    *(u32 *)(s + n - 8)  = c32;
    if (n <= 24) return dest;
    *(u32 *)(s + 12)     = c32;
    *(u32 *)(s + 16)     = c32;
    *(u32 *)(s + 20)     = c32;
    *(u32 *)(s + 24)     = c32;
    *(u32 *)(s + n - 28) = c32;
    *(u32 *)(s + n - 24) = c32;
    *(u32 *)(s + n - 20) = c32;
    *(u32 *)(s + n - 16) = c32;

    k  = 24 + ((uintptr_t)s & 4);
    s += k;
    n -= k;

    u64 c64 = c32 | ((u64)c32 << 32);
    for (; n >= 32; n -= 32, s += 32) {
        *(u64 *)(s + 0)  = c64;
        *(u64 *)(s + 8)  = c64;
        *(u64 *)(s + 16) = c64;
        *(u64 *)(s + 24) = c64;
    }
#else
    /* Pure C fallback with no aliasing violations. */
    for (; n; n--, s++)
        *s = c;
#endif

    return dest;
}

void *neo_acpi_memcpy(void *restrict dest, const void *restrict src, size_t n) {
    unsigned char       *d = dest;
    const unsigned char *s = src;

#ifdef __GNUC__

#    if __BYTE_ORDER == __LITTLE_ENDIAN
#        define LS >>
#        define RS <<
#    else
#        define LS <<
#        define RS >>
#    endif

    typedef uint32_t __attribute__((__may_alias__)) u32;
    uint32_t                                        w, x;

    for (; (uintptr_t)s % 4 && n; n--)
        *d++ = *s++;

    if ((uintptr_t)d % 4 == 0) {
        for (; n >= 16; s += 16, d += 16, n -= 16) {
            *(u32 *)(d + 0)  = *(u32 *)(s + 0);
            *(u32 *)(d + 4)  = *(u32 *)(s + 4);
            *(u32 *)(d + 8)  = *(u32 *)(s + 8);
            *(u32 *)(d + 12) = *(u32 *)(s + 12);
        }
        if (n & 8) {
            *(u32 *)(d + 0)  = *(u32 *)(s + 0);
            *(u32 *)(d + 4)  = *(u32 *)(s + 4);
            d               += 8;
            s               += 8;
        }
        if (n & 4) {
            *(u32 *)(d + 0)  = *(u32 *)(s + 0);
            d               += 4;
            s               += 4;
        }
        if (n & 2) {
            *d++ = *s++;
            *d++ = *s++;
        }
        if (n & 1) { *d = *s; }
        return dest;
    }

    if (n >= 32) switch ((uintptr_t)d % 4) {
        case 1:
            w     = *(u32 *)s;
            *d++  = *s++;
            *d++  = *s++;
            *d++  = *s++;
            n    -= 3;
            for (; n >= 17; s += 16, d += 16, n -= 16) {
                x                = *(u32 *)(s + 1);
                *(u32 *)(d + 0)  = (w LS 24) | (x RS 8);
                w                = *(u32 *)(s + 5);
                *(u32 *)(d + 4)  = (x LS 24) | (w RS 8);
                x                = *(u32 *)(s + 9);
                *(u32 *)(d + 8)  = (w LS 24) | (x RS 8);
                w                = *(u32 *)(s + 13);
                *(u32 *)(d + 12) = (x LS 24) | (w RS 8);
            }
            break;
        case 2:
            w     = *(u32 *)s;
            *d++  = *s++;
            *d++  = *s++;
            n    -= 2;
            for (; n >= 18; s += 16, d += 16, n -= 16) {
                x                = *(u32 *)(s + 2);
                *(u32 *)(d + 0)  = (w LS 16) | (x RS 16);
                w                = *(u32 *)(s + 6);
                *(u32 *)(d + 4)  = (x LS 16) | (w RS 16);
                x                = *(u32 *)(s + 10);
                *(u32 *)(d + 8)  = (w LS 16) | (x RS 16);
                w                = *(u32 *)(s + 14);
                *(u32 *)(d + 12) = (x LS 16) | (w RS 16);
            }
            break;
        case 3:
            w     = *(u32 *)s;
            *d++  = *s++;
            n    -= 1;
            for (; n >= 19; s += 16, d += 16, n -= 16) {
                x                = *(u32 *)(s + 3);
                *(u32 *)(d + 0)  = (w LS 8) | (x RS 24);
                w                = *(u32 *)(s + 7);
                *(u32 *)(d + 4)  = (x LS 8) | (w RS 24);
                x                = *(u32 *)(s + 11);
                *(u32 *)(d + 8)  = (w LS 8) | (x RS 24);
                w                = *(u32 *)(s + 15);
                *(u32 *)(d + 12) = (x LS 8) | (w RS 24);
            }
            break;
        }
    if (n & 16) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
    }
    if (n & 8) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
    }
    if (n & 4) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
    }
    if (n & 2) {
        *d++ = *s++;
        *d++ = *s++;
    }
    if (n & 1) { *d = *s; }
    return dest;
#endif

    for (; n; n--)
        *d++ = *s++;
    return dest;
}

#ifdef ALIGN
#    undef ALIGN
#endif

#define ALIGN (sizeof(size_t))

#define ONES       ((size_t)-1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) (((x) - ONES) & ~(x) & HIGHS)

size_t neo_acpi_strlen(const char *s) {
    const char *a = s;
    size_t     *w = NULL;
    for (; (uintptr_t)s % ALIGN; s++)
        if (!*s) return s - a;
    for (w = (void *)s; !HASZERO(*w); w++)
        ;
    for (s = (const void *)w; *s; s++)
        ;
    return s - a;
}

static bool is_char(char c, enum char_type type) {
    return (ascii_map[(uint8_t)c] & type) == type;
}

static char to_lower(char c) {
    if (is_char(c, CHAR_TYPE_UPPER)) return c + ('a' - 'A');

    return c;
}

static bool peek_one(const char **str, const size_t *size, char *out_char) {
    if (*size == 0) return false;

    *out_char = **str;
    return true;
}

static bool consume_one(const char **str, size_t *size, char *out_char) {
    if (!peek_one(str, size, out_char)) return false;

    *str  += 1;
    *size -= 1;
    return true;
}

static bool consume_if(const char **str, size_t *size, enum char_type type) {
    char c;

    if (!peek_one(str, size, &c) || !is_char(c, type)) return false;

    *str  += 1;
    *size -= 1;
    return true;
}

static bool consume_if_equals(const char **str, size_t *size, char c) {
    char c1;

    if (!peek_one(str, size, &c1) || to_lower(c1) != c) return false;

    *str  += 1;
    *size -= 1;
    return true;
}

bool neo_acpi_string_to_integer(const char *str, size_t max_chars, enum neo_acpi_base base,
                                uint64_t *out_value) {
    bool     ret      = false;
    bool     negative = false;
    uint64_t next, value = 0;
    char     c = '\0';

    while (consume_if(&str, &max_chars, CHAR_TYPE_SPACE))
        ;

    if (consume_if_equals(&str, &max_chars, '-'))
        negative = true;
    else
        consume_if_equals(&str, &max_chars, '+');

    if (base == NEO_ACPI_BASE_AUTO) {
        base = NEO_ACPI_BASE_DEC;

        if (consume_if_equals(&str, &max_chars, '0')) {
            base = NEO_ACPI_BASE_OCT;
            if (consume_if_equals(&str, &max_chars, 'x')) base = NEO_ACPI_BASE_HEX;
        }
    }

    while (consume_one(&str, &max_chars, &c)) {
        switch (ascii_map[(uint8_t)c] & (CHAR_TYPE_DIGIT | CHAR_TYPE_ALHEX)) {
        case CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT:
            next = c - '0';
            if (base == NEO_ACPI_BASE_OCT && next > 7) goto out;
            break;
        case CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT:
        case CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT:
            if (base != NEO_ACPI_BASE_HEX) goto out;
            next = 10 + (to_lower(c) - 'a');
            break;
        default: goto out;
        }

        next = (value * base) + next;
        if ((next / base) != value) {
            value = 0xFFFFFFFFFFFFFFFF;
            goto out;
        }

        value = next;
    }

out:
    if (negative) value = -((int64_t)value);

    *out_value = value;
    if (max_chars == 0 || c == '\0') ret = true;

    return ret;
}

static void write_one(struct fmt_buf_state *fb_state, char c) {
    if (fb_state->bytes_written < fb_state->capacity) fb_state->buffer[fb_state->bytes_written] = c;

    fb_state->bytes_written++;
}

static void write_many(struct fmt_buf_state *fb_state, const char *string, size_t count) {
    if (fb_state->bytes_written < fb_state->capacity) {
        size_t count_to_write;

        count_to_write = NEO_ACPI_MIN(count, fb_state->capacity - fb_state->bytes_written);
        neo_acpi_memcpy(&fb_state->buffer[fb_state->bytes_written], string, count_to_write);
    }

    fb_state->bytes_written += count;
}

static void write_padding(struct fmt_buf_state *fb_state, struct fmt_spec *fm, size_t repr_size) {
    uint64_t mw = fm->min_width;

    if (mw <= repr_size) return;

    mw -= repr_size;

    while (mw--)
        write_one(fb_state, fm->left_justify ? ' ' : fm->pad_char);
}

static char hex_char(bool upper, uint64_t value) {
    static const char upper_hex[] = "0123456789ABCDEF";
    static const char lower_hex[] = "0123456789abcdef";

    return (upper ? upper_hex : lower_hex)[value];
}

static bool string_has_at_least(const char *string, size_t characters) {
    while (*string) {
        if (--characters == 0) return true;

        string++;
    }

    return false;
}

static bool consume_digits(const char **string, size_t *out_size) {
    size_t size = 0;

    for (;;) {
        char c = **string;
        if (c < '0' || c > '9') break;

        size++;
        *string += 1;
    }

    if (size == 0) return false;

    *out_size = size;
    return true;
}

static bool parse_number(const char **fmt, enum parse_number_mode mode, uint64_t *out_value) {
    size_t      num_digits;
    const char *digits = *fmt;
    if (!consume_digits(fmt, &num_digits)) return mode != PARSE_NUMBER_MODE_MUST;
    return neo_acpi_string_to_integer(digits, num_digits, NEO_ACPI_BASE_DEC, out_value);
}

static void write_integer(struct fmt_buf_state *fb_state, struct fmt_spec *fm, uint64_t value) {
    char     repr_buffer[REPR_BUFFER_SIZE];
    size_t   index = REPR_BUFFER_SIZE;
    uint64_t remainder;
    char     repr;
    bool     negative = false;
    size_t   repr_size;

    if (fm->is_signed) {
        int64_t as_ll = value;

        if (as_ll < 0) {
            value    = -as_ll;
            negative = true;
        }
    }

    if (fm->prepend || negative) write_one(fb_state, negative ? '-' : fm->prepend_char);

    while (value) {
        remainder  = value % fm->base;
        value     /= fm->base;

        if (fm->base == 16) {
            repr = hex_char(fm->uppercase, remainder);
        } else if (fm->base == 8 || fm->base == 10) {
            repr = remainder + '0';
        } else {
            repr = '?';
        }

        repr_buffer[--index] = repr;
    }
    repr_size = REPR_BUFFER_SIZE - index;

    if (repr_size == 0) {
        repr_buffer[--index] = '0';
        repr_size            = 1;
    }

    if (fm->alternate_form) {
        if (fm->base == 16) {
            repr_buffer[--index]  = fm->uppercase ? 'X' : 'x';
            repr_buffer[--index]  = '0';
            repr_size            += 2;
        } else if (fm->base == 8) {
            repr_buffer[--index]  = '0';
            repr_size            += 1;
        }
    }

    if (fm->left_justify) {
        write_many(fb_state, &repr_buffer[index], repr_size);
        write_padding(fb_state, fm, repr_size);
    } else {
        write_padding(fb_state, fm, repr_size);
        write_many(fb_state, &repr_buffer[index], repr_size);
    }
}

static bool consume(const char **string, const char *token) {
    size_t token_size;

    token_size = neo_acpi_strlen(token);

    if (!string_has_at_least(*string, token_size)) return false;

    if (!neo_acpi_memcmp(*string, token, token_size)) {
        *string += token_size;
        return true;
    }

    return false;
}

static bool is_one_of(char c, const char *list) {
    for (; *list; list++) {
        if (c == *list) return true;
    }
    return false;
}

static bool consume_one_of(const char **string, const char *list, char *consumed_char) {
    char c = **string;
    if (!c) return false;

    if (is_one_of(c, list)) {
        *consumed_char  = c;
        *string        += 1;
        return true;
    }

    return false;
}

static uint32_t base_from_specifier(char specifier) {
    switch (specifier) {
    case 'x':
    case 'X': return 16;
    case 'o': return 8;
    default: return 10;
    }
}

static bool is_uppercase_specifier(char specifier) {
    return specifier == 'X';
}

static const char *find_next_conversion(const char *fmt, size_t *offset) {
    *offset = 0;

    while (*fmt) {
        if (*fmt == '%') return fmt;

        fmt++;
        *offset += 1;
    }

    return NULL;
}

int32_t neo_acpi_vsnprintf(char *buffer, size_t capacity, const char *fmt, neo_acpi_va_list vlist) {
    struct fmt_buf_state fb_state = {0};
    uint64_t             value;
    const char          *next_conversion;
    size_t               next_offset;
    char                 flag;

    fb_state.buffer        = buffer;
    fb_state.capacity      = capacity;
    fb_state.bytes_written = 0;

    while (*fmt) {
        struct fmt_spec fm = {
            .pad_char = ' ',
            .base     = 10,
        };
        next_conversion = find_next_conversion(fmt, &next_offset);

        if (next_offset) write_many(&fb_state, fmt, next_offset);

        if (!next_conversion) break;

        fmt = next_conversion;
        if (consume(&fmt, "%%")) {
            write_one(&fb_state, '%');
            continue;
        }

        // consume %
        fmt++;

        while (consume_one_of(&fmt, "+- 0#", &flag)) {
            switch (flag) {
            case '+':
            case ' ':
                fm.prepend      = true;
                fm.prepend_char = flag;
                continue;
            case '-': fm.left_justify = true; continue;
            case '0': fm.pad_char = '0'; continue;
            case '#': fm.alternate_form = true; continue;
            default: return -1;
            }
        }

        if (consume(&fmt, "*")) {
            fm.min_width = neo_acpi_va_arg(vlist, int);
        } else if (!parse_number(&fmt, PARSE_NUMBER_MODE_MAYBE, &fm.min_width)) {
            return -1;
        }

        if (consume(&fmt, ".")) {
            fm.has_precision = true;

            if (consume(&fmt, "*")) {
                fm.precision = neo_acpi_va_arg(vlist, int);
            } else {
                if (!parse_number(&fmt, PARSE_NUMBER_MODE_MUST, &fm.precision)) return -1;
            }
        }

        flag = 0;

        if (consume(&fmt, "c")) {
            char c = neo_acpi_va_arg(vlist, int);
            write_one(&fb_state, c);
            continue;
        }

        if (consume(&fmt, "s")) {
            const char *string = neo_acpi_va_arg(vlist, char *);
            size_t      i;

            if (string == NULL) string = "<null>";

            for (i = 0; (!fm.has_precision || i < fm.precision) && string[i]; ++i)
                write_one(&fb_state, string[i]);
            while (i++ < fm.min_width)
                write_one(&fb_state, ' ');
            continue;
        }

        if (consume(&fmt, "p")) {
            value        = (uintptr_t)neo_acpi_va_arg(vlist, void *);
            fm.base      = 16;
            fm.min_width = _POINTER_SIZE * 2;
            fm.pad_char  = '0';
            goto write_int;
        }

        if (consume(&fmt, "hh")) {
            if (consume(&fmt, "d") || consume(&fmt, "i")) {
                value        = (signed char)neo_acpi_va_arg(vlist, int);
                fm.is_signed = true;
            } else if (consume_one_of(&fmt, "oxXu", &flag)) {
                value = (unsigned char)neo_acpi_va_arg(vlist, int);
            } else {
                return -1;
            }
            goto write_int;
        }

        if (consume(&fmt, "h")) {
            if (consume(&fmt, "d") || consume(&fmt, "i")) {
                value        = (signed short)neo_acpi_va_arg(vlist, int);
                fm.is_signed = true;
            } else if (consume_one_of(&fmt, "oxXu", &flag)) {
                value = (unsigned short)neo_acpi_va_arg(vlist, int);
            } else {
                return -1;
            }
            goto write_int;
        }

        if (consume(&fmt, "ll") || (sizeof(size_t) == sizeof(long long) && consume(&fmt, "z"))) {
            if (consume(&fmt, "d") || consume(&fmt, "i")) {
                value        = neo_acpi_va_arg(vlist, long long);
                fm.is_signed = true;
            } else if (consume_one_of(&fmt, "oxXu", &flag)) {
                value = neo_acpi_va_arg(vlist, unsigned long long);
            } else {
                return -1;
            }
            goto write_int;
        }

        if (consume(&fmt, "l") || (sizeof(size_t) == sizeof(long) && consume(&fmt, "z"))) {
            if (consume(&fmt, "d") || consume(&fmt, "i")) {
                value        = neo_acpi_va_arg(vlist, long);
                fm.is_signed = true;
            } else if (consume_one_of(&fmt, "oxXu", &flag)) {
                value = neo_acpi_va_arg(vlist, unsigned long);
            } else {
                return -1;
            }
            goto write_int;
        }

        if (consume(&fmt, "d") || consume(&fmt, "i")) {
            value        = neo_acpi_va_arg(vlist, int32_t);
            fm.is_signed = true;
        } else if (consume_one_of(&fmt, "oxXu", &flag)) {
            value = neo_acpi_va_arg(vlist, uint32_t);
        } else {
            return -1;
        }

    write_int:
        if (flag != 0) {
            fm.base      = base_from_specifier(flag);
            fm.uppercase = is_uppercase_specifier(flag);
        }

        write_integer(&fb_state, &fm, value);
    }

    if (fb_state.capacity) {
        size_t last_char;

        last_char                  = NEO_ACPI_MIN(fb_state.bytes_written, fb_state.capacity - 1);
        fb_state.buffer[last_char] = '\0';
    }

    return fb_state.bytes_written;
}
