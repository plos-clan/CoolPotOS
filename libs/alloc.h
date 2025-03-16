#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

bool heap_init(uint8_t *address, size_t size);

void *malloc(size_t size);

void free(void *ptr);

void* alloc_4k_aligned_mem(size_t size);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
