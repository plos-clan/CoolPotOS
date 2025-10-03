#pragma once

#include "ctype.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

bool heap_init(uint8_t *address, size_t size);

void *malloc(size_t size);

void free(void *ptr);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
