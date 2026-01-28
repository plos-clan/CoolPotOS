#ifndef __ACCPOS_H__
#define __ACCPOS_H__

#include "krlibc.h"
#include "types/stdarg.h"
#include "types.h"

/* Avoid symbol clashes with kernel sprintf/snprintf/vsnprintf. */
#define sprintf   acpi_sprintf
#define snprintf  acpi_snprintf
#define vsnprintf acpi_vsnprintf

int acpi_vsnprintf(char *String, size_t Size, const char *Format, va_list Args);
int acpi_snprintf(char *String, size_t Size, const char *Format, ...);
int acpi_sprintf(char *String, const char *Format, ...);

/*
 * CoolPotOS ACPICA configuration (freestanding kernel build).
 * Avoid standard headers; rely on kernel-provided libc routines.
 */
#ifndef ACPI_USE_SYSTEM_CLIBRARY
#    define ACPI_USE_SYSTEM_CLIBRARY
#endif
#define ACPI_USE_DO_WHILE_0
#define ACPI_IGNORE_PACKAGE_RESOLUTION_ERRORS

/* ACPICA global variable declaration helpers */
#ifdef DEFINE_ACPI_GLOBALS
#    define ACPI_GLOBAL(type, name) type name
#    define ACPI_INIT_GLOBAL(type, name, value) type name = value
#else
#    ifndef ACPI_GLOBAL
#        define ACPI_GLOBAL(type, name) extern type name
#    endif
#    ifndef ACPI_INIT_GLOBAL
#        define ACPI_INIT_GLOBAL(type, name, value) extern type name
#    endif
#endif

/* External interface stubs used by hardware-dependent headers */
#ifndef ACPI_EXTERNAL_RETURN_STATUS
#    define ACPI_EXTERNAL_RETURN_STATUS(Prototype) Prototype;
#endif

#ifndef ACPI_EXTERNAL_RETURN_OK
#    define ACPI_EXTERNAL_RETURN_OK(Prototype) Prototype;
#endif

#ifndef ACPI_EXTERNAL_RETURN_VOID
#    define ACPI_EXTERNAL_RETURN_VOID(Prototype) Prototype;
#endif

#ifndef ACPI_EXTERNAL_RETURN_UINT32
#    define ACPI_EXTERNAL_RETURN_UINT32(Prototype) Prototype;
#endif

#ifndef ACPI_DBR_DEPENDENT_RETURN_OK
#    ifdef ACPI_DEBUGGER
#        define ACPI_DBR_DEPENDENT_RETURN_OK(Prototype) ACPI_EXTERNAL_RETURN_OK(Prototype)
#    else
#        define ACPI_DBR_DEPENDENT_RETURN_OK(Prototype)                                      \
            static ACPI_INLINE Prototype { return (AE_OK); }
#    endif
#endif

#ifndef ACPI_DBR_DEPENDENT_RETURN_VOID
#    ifdef ACPI_DEBUGGER
#        define ACPI_DBR_DEPENDENT_RETURN_VOID(Prototype) ACPI_EXTERNAL_RETURN_VOID(Prototype)
#    else
#        define ACPI_DBR_DEPENDENT_RETURN_VOID(Prototype)                                    \
            static ACPI_INLINE Prototype { return; }
#    endif
#endif

#if (!ACPI_REDUCED_HARDWARE)
#    define ACPI_HW_DEPENDENT_RETURN_STATUS(Prototype) ACPI_EXTERNAL_RETURN_STATUS(Prototype)
#    define ACPI_HW_DEPENDENT_RETURN_OK(Prototype)     ACPI_EXTERNAL_RETURN_OK(Prototype)
#    define ACPI_HW_DEPENDENT_RETURN_UINT32(Prototype) ACPI_EXTERNAL_RETURN_UINT32(Prototype)
#    define ACPI_HW_DEPENDENT_RETURN_VOID(Prototype)   ACPI_EXTERNAL_RETURN_VOID(Prototype)
#else
#    define ACPI_HW_DEPENDENT_RETURN_STATUS(Prototype)                                    \
        static ACPI_INLINE Prototype { return (AE_NOT_CONFIGURED); }
#    define ACPI_HW_DEPENDENT_RETURN_OK(Prototype)                                        \
        static ACPI_INLINE Prototype { return (AE_OK); }
#    define ACPI_HW_DEPENDENT_RETURN_UINT32(Prototype)                                    \
        static ACPI_INLINE Prototype { return (0); }
#    define ACPI_HW_DEPENDENT_RETURN_VOID(Prototype)                                      \
        static ACPI_INLINE Prototype { return; }
#endif

/* Host-dependent types and defines */
#if defined(__x86_64__) || defined(__aarch64__) || defined(__loongarch64__) ||                   \
    (defined(__riscv) && (defined(__LP64__) || defined(_LP64))) || defined(__PPC64__) ||         \
    defined(__s390x__) || defined(__ia64__)
#    define ACPI_MACHINE_WIDTH        64
#    define COMPILER_DEPENDENT_INT64  long
#    define COMPILER_DEPENDENT_UINT64 unsigned long
#else
#    define ACPI_MACHINE_WIDTH        32
#    define COMPILER_DEPENDENT_INT64  long long
#    define COMPILER_DEPENDENT_UINT64 unsigned long long
#    define ACPI_USE_NATIVE_DIVIDE
#    define ACPI_USE_NATIVE_MATH64
#endif

#define ACPI_UINTPTR_T uintptr_t
#define ACPI_OFFSET(d, f)  offsetof(d, f)

#ifndef __cdecl
#    define __cdecl
#endif

#ifndef ACPI_INIT_FUNCTION
#    define ACPI_INIT_FUNCTION
#endif

#ifndef ACPI_STRUCT_INIT
#    define ACPI_STRUCT_INIT(field, value) .field = value
#endif

/* Minimal ctype helpers for ACPICA when standard headers are unavailable */
#ifndef isupper
static inline int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}
#endif

#ifndef islower
static inline int islower(int c) {
    return (c >= 'a' && c <= 'z');
}
#endif

#ifndef isalpha
static inline int isalpha(int c) {
    return isupper(c) || islower(c);
}
#endif

#ifndef isprint
static inline int isprint(int c) {
    return (c >= 0x20 && c <= 0x7e);
}
#endif

#ifndef toupper
static inline int toupper(int c) {
    return islower(c) ? (c - 'a' + 'A') : c;
}
#endif

#ifndef tolower
static inline int tolower(int c) {
    return isupper(c) ? (c - 'A' + 'a') : c;
}
#endif

#ifndef strncat
static inline char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (*d) {
        d++;
    }
    while (n-- && *src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dst;
}
#endif

#endif /* __ACCPOS_H__ */
