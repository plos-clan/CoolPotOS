#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef __INTPTR_TYPE__ ssize_t;
typedef __INTPTR_TYPE__ ptrdiff_t;
typedef size_t          usize;
typedef ssize_t         isize;

typedef void (*free_t)(void *ptr);

#define loop while (true)

#define __attr(...) __attribute__((__VA_ARGS__))
#define __wur       __attribute__((warn_unused_result))
#define _rest       __restrict
#define __THROW     __attr(nothrow, leaf)

// 函数返回内存地址，调用者获得内存所有权
#define ownership_returns(type)       __attr(ownership_returns(type))
// 函数获取内存所有权并释放内存
#define ownership_takes(type, nparam) __attr(ownership_takes(type, nparam))
// 函数获取内存所有权
#define ownership_holds(type, nparam) __attr(ownership_holds(type, nparam))

#ifdef __clang__
#    define __attr_dealloc(func, nparam) __attr_malloc
#else
#    define __attr_dealloc(func, nparam) __attr(warn_unused_result, malloc(func, nparam))
#endif
#define __attr_malloc         __attr(warn_unused_result, malloc)
#define __attr_allocsize(...) __attr(alloc_size(__VA_ARGS__))
#define __nnull(...)          __attr(nonnull(__VA_ARGS__))

#define USED           __attr(used)
#define SECTION(name)  __attr(section(name))
#define LIMINE_REQUEST USED SECTION(".limine_requests") static volatile
