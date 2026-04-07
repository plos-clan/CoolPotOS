#pragma once

#include "types.h"

typedef enum HeapError {
    InvalidFree,
    LayoutError,
} HeapError;

/**
 * Type alias for the error handler function pointer.
 */
typedef void (*ErrorHandler)(enum HeapError error, void *ptr);

typedef struct MemorySpan {
    /**
     * Pointer to the start of the new memory block.
     * Must be non-null and properly aligned if the system requires it.
     */
    uint8_t *ptr;
    /**
     * The size of the memory block in bytes.
     * If `size` is 0, the allocator considers the OOM handling failed.
     */
    size_t size;
} MemorySpan;

typedef struct MemorySpan (*OomCallback)(size_t);

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Initializes (or RESETS) the heap memory arena.
 *
 * # Warning
 * Calling this function a second time will **wipe** the allocator state.
 * Any pointers allocated before the reset will become "leaked" (safe to use,
 * but calling free() on them later is Undefined Behavior/Double Free because
 * the new allocator doesn't know about them).
 *
 * # Safety
 * - `address` must be a valid pointer to the start of a contiguous, writable memory block.
 * - `size` must be the correct size of that memory block in bytes.
 * - The memory range `[address, address + size)` must be exclusively available to the allocator
 *   (it must not be accessed or modified by other code while managed by the allocator).
 * - Thread safety is handled internally by the allocator lock.
 */
bool heap_init(uint8_t *address, size_t size);

/**
 * Extends the heap with a new memory block.
 *
 * # Safety
 * - `address` must be a valid pointer to the start of a new, available memory block.
 * - `size` must be the correct size of that memory block.
 * - The memory block must not overlap with currently managed memory (unless extending the end).
 * - Thread safety is handled internally by the allocator lock.
 */
bool heap_extend(uint8_t *address, size_t size);

/**
 * Sets a custom error handler function to be called on heap errors.
 * Passing `None` clears any previously set handler.
 *
 * # Safety
 * - The `handler` function pointer must be valid and callable.
 */
void heap_onerror(ErrorHandler handler);

/**
 * Registers a custom Out-Of-Memory (OOM) handler.
 *
 * # Purpose
 * This handler acts as a hook that is triggered when the allocator **runs out of memory**.
 * It provides a mechanism to **automatically extend the heap** on demand.
 *
 * Instead of immediately returning `NULL` when the heap is full, the allocator will:
 * 1. Call this function.
 * 2. If this function returns a valid new memory span, the allocator adds it to the heap.
 * 3. The allocator retries the original allocation request.
 *
 * # Safety
 * - The `callback` function pointer must be valid and callable.
 * - The callback implementation must ensure the returned memory is valid and not already in use.
 * - The callback itself must be thread-safe if the allocator is accessed concurrently.
 */
void heap_set_oom_handler(OomCallback callback);

/**
 * Returns the usable size of the memory block pointed to by `ptr`.
 * This corresponds to the size originally requested during allocation (`malloc`, `aligned_alloc`,
 * `realloc`). Returns 0 if `ptr` is null.
 *
 * # Safety
 * - `ptr` must be null or a pointer previously returned by `malloc`, `realloc`,
 *   or `aligned_alloc` from this specific allocator instance and implementation.
 *   Passing any other pointer (including pointers offset from the original user pointer)
 *   leads to Undefined Behavior.
 * - The behavior is undefined if the metadata preceding `ptr` has been corrupted.
 */
size_t usable_size(void *ptr);

void *liballoc_malloc(size_t size);
void *liballoc_calloc(size_t nmemb, size_t size);
void *liballoc_aligned_alloc(size_t alignment, size_t size);
void liballoc_free(void *ptr);
void *liballoc_realloc(void *ptr, size_t size);

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
 * Allocates memory for an array of `nmemb` elements of `size` bytes each
 * and initializes all bytes in the allocated storage to zero.
 *
 * # Safety
 * Caller is responsible for handling the returned pointer and freeing it.
 * Returns NULL on integer overflow or allocation failure.
 */
void *calloc(size_t nmemb, size_t size);

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
 * - `size` is the desired size for the new allocation.
 * - If reallocation fails, the original pointer `ptr` remains valid and must still be freed.
 * - If reallocation succeeds, the original `ptr` is invalidated and the new pointer should be used.
 */
void *realloc(void *ptr, size_t size);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
