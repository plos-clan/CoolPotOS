#pragma once

#include "ctype.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Initializes the heap memory arena.
 *
 * # Safety
 * - `address` must be a valid pointer to the start of a memory block.
 * - `size` must be the correct size of that memory block.
 * - This function should only be called once.
 * - The memory block should ideally be aligned to at least `align_of::<Metadata>()`.
 */
bool heap_init(uint8_t *address, size_t size);

/**
 * Allocates memory with default alignment (`align_of::<usize>`).
 * Stores metadata (size, alignment) before the returned pointer.
 *
 * # Safety
 * Caller is responsible for handling the returned pointer (e.g., checking for null)
 * and eventually freeing it with `free`.
 */
void *malloc(size_t size);

/**
 * Allocates memory with specified alignment.
 * Stores metadata (size, alignment) before the returned pointer.
 *
 * # Safety
 * Caller is responsible for handling the returned pointer and eventually freeing it.
 * `alignment` must be a power of two.
 * `size` must be non-zero (as per C standard `aligned_alloc`).
 * Behavior is undefined if `size` is not a multiple of `alignment` (C standard requirement).
 */
void *aligned_alloc(size_t alignment, size_t size);

/**
 * Frees memory previously allocated by `malloc`, `realloc`, or `aligned_alloc`.
 *
 * # Safety
 * - `ptr` must be null or a pointer previously returned by `malloc`, `realloc`, or `aligned_alloc`
 *   from *this* allocator instance.
 * - Calling `free` multiple times on the same non-null pointer leads to double-free (UB).
 * - Using the pointer after `free` leads to use-after-free (UB).
 */
void free(void *ptr);

/**
 * Reallocates memory previously allocated by `malloc`, `realloc`, or `aligned_alloc`.
 * Attempts to use `talc`'s underlying realloc for efficiency, preserving the original alignment.
 *
 * # Safety
 * - `ptr` must be null or a pointer previously returned by `malloc`, `realloc`, or `aligned_alloc`.
 * - `new_size` is the desired size for the new allocation.
 * - If reallocation fails, the original pointer `ptr` remains valid and must still be freed.
 * - If reallocation succeeds, the original `ptr` is invalidated and the new pointer should be used.
 */
void *realloc(void *ptr, size_t new_size);

/**
 * Returns the usable size of the memory block pointed to by `ptr`.
 * This corresponds to the size originally requested during allocation (`malloc`, `aligned_alloc`, `realloc`).
 * Returns 0 if `ptr` is null.
 *
 * # Safety
 * - `ptr` must be null or a pointer previously returned by `malloc`, `realloc`,
 *   or `aligned_alloc` from this specific allocator instance and implementation.
 *   Passing any other pointer (including pointers offset from the original user pointer)
 *   leads to Undefined Behavior.
 * - The behavior is undefined if the metadata preceding `ptr` has been corrupted.
 */
size_t usable_size(void *ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
