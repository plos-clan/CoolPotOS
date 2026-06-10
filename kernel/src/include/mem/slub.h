#pragma once

#include "types.h"

void slub_init(void);

void *malloc(size_t size) __attribute__((nothrow, leaf)) __attribute__((warn_unused_result));
void free(void *ptr) __attribute__((nothrow, leaf));
void *realloc(void *ptr, size_t size) __attribute__((nothrow, leaf))
__attribute__((warn_unused_result));
void *aligned_alloc(size_t alignment, size_t size) __attribute__((nothrow, leaf))
__attribute__((warn_unused_result));
void *calloc(size_t nmemb, size_t size) __attribute__((nothrow, leaf))
__attribute__((warn_unused_result));
