#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef __INTPTR_TYPE__ ssize_t;
typedef __INTPTR_TYPE__ ptrdiff_t;
typedef size_t          usize;
typedef ssize_t         isize;

typedef void (*free_t)(void *ptr);

#define __wur __attribute__((warn_unused_result))
#define _rest __restrict
