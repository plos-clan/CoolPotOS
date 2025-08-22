#pragma once

#include <mem_subsystem.h>

void qsort_swap(void *a, void *b, size_t size);

void *qsort_partition(void *base, size_t size, void *low, void *high,
                      int (*cmp)(const void *, const void *));

void qsort_quicksort(void *base, size_t size, void *low, void *high,
                     int (*cmp)(const void *, const void *));

void qsort(void *base, size_t nitems, size_t size, int (*cmp)(const void *, const void *));

int qsort_compare(const void *a, const void *b);

void *memmove(void *dest, const void *src, size_t num);

/**
 * Status codes for relative path calculation
 */
typedef enum {
    REL_SUCCESS                = 0,
    REL_ERROR_INVALID          = -1,
    REL_ERROR_NO_COMMON_PREFIX = -2,
    REL_ERROR_MEMORY           = -3,
    REL_ERROR_NOT_ABSOLUTE     = -4
} rel_status;

/**
 * Calculate relative path from one absolute path to another
 * @param relative Output buffer for relative path
 * @param from Source absolute path
 * @param to Target absolute path
 * @param size Size of output buffer
 * @return rel_status code indicating success or specific error
 */
rel_status calculate_relative_path(char *relative, const char *from, const char *to, size_t size);
