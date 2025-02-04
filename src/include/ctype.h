#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef __INTPTR_TYPE__ ssize_t;
typedef __INTPTR_TYPE__ ptrdiff_t;
typedef size_t usize;
typedef ssize_t isize;

typedef void (*free_t)(void *ptr);

#define _rest __restrict
