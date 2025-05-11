#pragma once

#pragma GCC system_header

#define NULL 0
#define loop while (true)

#ifndef __cplusplus
#    if __STDC_VERSION__ < 202311L
#        define bool  _Bool
#        define true  ((bool)1)
#        define false ((bool)0)
#    else
#        define _Bool typeof("Who let u fuck use _Bool?!"[0][0])
#    endif
#endif

#if __INT_WIDTH__ != 32
#    error "int must be int32"
#endif

#if __FLT_RADIX__ != 2
#    error "float must be binary"
#endif

#if __FLT_MANT_DIG__ != 24
#    error "float must be 32 bit float"
#endif

#if __DBL_MANT_DIG__ != 53
#    error "double must be 64 bit float"
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~ 基本类型

typedef __INTPTR_TYPE__  intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__  ssize_t;
typedef __UINTPTR_TYPE__ size_t;
typedef __INTPTR_TYPE__  ptrdiff_t;
typedef size_t           usize;
typedef ssize_t          isize;

typedef signed char        schar; // 在大多数环境下 schar 就是 char
typedef unsigned char      uchar;
typedef unsigned short     ushort;
typedef unsigned int       uint;
typedef unsigned long      ulong;
typedef long long          llong;
typedef unsigned long long ullong;

#ifndef __cplusplus
typedef __UINT8_TYPE__ char8_t; // 我们认为 char8 就是 uint8
typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;
typedef __WCHAR_TYPE__ wchar_t;
#endif

typedef __INT8_TYPE__   int8_t;
typedef __UINT8_TYPE__  uint8_t;
typedef __INT16_TYPE__  int16_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __INT32_TYPE__  int32_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT64_TYPE__  int64_t;
typedef __UINT64_TYPE__ uint64_t;
typedef float           float32_t;
typedef double          float64_t;
#if defined(__x86_64__)
typedef __int128          int128_t;
typedef unsigned __int128 uint128_t;
#    if !NO_EXTFLOAT
typedef _Float16   float16_t; // 命名最不统一的一集
typedef __float128 float128_t;
#    endif
#endif

typedef __INTMAX_TYPE__  intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

typedef int     ssize_t;
typedef size_t  usize;
typedef ssize_t isize;

typedef void (*free_t)(void *ptr);

typedef void *(*alloc_t)(size_t size);

#define __PACKED__   __attribute__((packed))
#define __ALIGN__(x) __attr(packed, aligned(x))
#define __ALIGN2__   __ALIGN__(2)
#define __ALIGN4__   __ALIGN__(4)
#define __ALIGN8__   __ALIGN__(8)
#define __ALIGN16__  __ALIGN__(16)

#define _rest __restrict

#define __wur             __attribute__((warn_unused_result))
#define __nonnull(params) __attribute__((nonnull params))
#define __attr(...)       __attribute__((__VA_ARGS__))
#define __attr_deprecated __attr(deprecated)
#define __attr_nthrow     __attr(nothrow)
#define __attr_leaf       __attr(leaf)
#define __attr_nnull(...) __attr(nonnull(__VA_ARGS__))
#define __attr_malloc     __attr(warn_unused_result, malloc)
#ifdef __clang__
#    define __attr_dealloc(func, nparam) __attr_malloc
#else
#    define __attr_dealloc(func, nparam) __attr(warn_unused_result, malloc(func, nparam))
#endif
#define __attr_allocsize(...) __attr(alloc_size(__VA_ARGS__))

#ifdef __cplusplus
#    define __THROW      noexcept(true)
#    define __THROWNL    noexcept(true)
#    define __NTH(fct)   __LEAF_ATTR fct __THROW
#    define __NTHNL(fct) fct __THROW
#else
#    define __THROW      __attr(nothrow, leaf)
#    define __THROWNL    __attr(nothrow)
#    define __NTH(fct)   __attr(nothrow, leaf) fct
#    define __NTHNL(fct) __attr(nothrow) fct
#endif

#define __attribute_pure__ __attr(pure)
#define __attr_pure        __attr(pure)
#ifdef __clang__
#    define __attr_access(x)
#    define __attr_readonly(...)
#    define __attr_writeonly(...)
#else
#    define __attr_access(x)      __attr(access x) // 这个 redefine 没有问题，忽略掉
#    define __attr_readonly(...)  __attr(access(read_only, ##__VA_ARGS__))
#    define __attr_writeonly(...) __attr(access(write_only, ##__VA_ARGS__))
#endif

#define __nnull(...) __attr(nonnull(__VA_ARGS__))

// C++ 属性
#ifndef __cplusplus
#    define noexcept
#endif

#define __deprecated __attr(deprecated)

#define USED __attr(used)

#define __nif __attr(no_instrument_function)

// __attribute__((overloadable)) 是 clang 扩展，使 C 函数可以被重载

#ifdef __cplusplus
#    define overload
#    define dlexport    __attr(used, visibility("default"))
#    define dlimport    extern
#    define dlimport_c  extern "C"
#    define dlimport_x  extern
#    define dlhidden    __attr(visibility("hidden"))
#    define dlinternal  __attr(visibility("internal"))
#    define dlprotected __attr(visibility("protected"))
#else
#    define overload    __attribute__((overloadable))
#    define dlexport    __attr(used, visibility("default"))
#    define dlimport    extern
#    define dlimport_c  extern
#    define dlimport_x  extern overload
#    define dlhidden    __attr(visibility("hidden"))
#    define dlinternal  __attr(visibility("internal"))
#    define dlprotected __attr(visibility("protected"))
#endif
#if DEBUG
#    define finline static __nif
#else
#    define finline static inline __attr(always_inline) __nif
#endif
