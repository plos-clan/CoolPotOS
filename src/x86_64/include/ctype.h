#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef __INTPTR_TYPE__ ssize_t;
typedef __INTPTR_TYPE__ ptrdiff_t;
typedef size_t          usize;
typedef ssize_t         isize;

typedef void (*free_t)(void *ptr);

#define infinite_loop while (true)

#define __attr(...) __attribute__((__VA_ARGS__))
#define __wur       __attribute__((warn_unused_result))
#define _rest       __restrict

#define USED           __attr(used)
#define SECTION(name)  __attr(section(name))
#define LIMINE_REQUEST USED SECTION(".limine_requests") static volatile
