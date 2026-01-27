#pragma once

#define NEO_ACPI_ABS(x)    ((x) > 0 ? (x) : -(x))
#define NEO_ACPI_MAX(x, y) ((x > y) ? (x) : (y))
#define NEO_ACPI_MIN(x, y) ((x < y) ? (x) : (y))

#define REPR_BUFFER_SIZE 32

#ifdef _WIN32
#    ifdef _WIN64
#        define _POINTER_SIZE 8
#    else
#        define _POINTER_SIZE 4
#    endif
#elif defined(__GNUC__)
#    define _POINTER_SIZE __SIZEOF_POINTER__
#elif defined(__WATCOMC__)
#    ifdef __386__
#        define _POINTER_SIZE 4
#    elif defined(__I86__)
#        error NeoACPI does not support 16-bit mode compilation
#    else
#        error Unknown target architecture
#    endif
#else
#    error Failed to detect pointer size
#endif

#include "neotype.h"

struct fmt_buf_state {
    char  *buffer;
    size_t capacity;
    size_t bytes_written;
};

struct fmt_spec {
    uint8_t  is_signed      : 1;
    uint8_t  prepend        : 1;
    uint8_t  uppercase      : 1;
    uint8_t  left_justify   : 1;
    uint8_t  alternate_form : 1;
    uint8_t  has_precision  : 1;
    char     pad_char;
    char     prepend_char;
    uint64_t min_width;
    uint64_t precision;
    uint32_t base;
};

char   *neo_acpi_strcat(char *dest, const char *src);
size_t  neo_acpi_strlen(const char *s);
void   *neo_acpi_memcpy(void *restrict dest, const void *restrict src, size_t n);
void   *neo_acpi_memset(void *dest, int c, size_t n);
int     neo_acpi_memcmp(const void *a_, const void *b_, size_t size);
int32_t neo_acpi_vsnprintf(char *buffer, size_t capacity, const char *fmt, neo_acpi_va_list vlist);
