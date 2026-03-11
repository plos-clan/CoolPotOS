#include "config.h"
#include "compat.h"
#include "util/array.h"
#include "sqfs/error.h"

int array_init(array_t *array, size_t size, size_t capacity) {
    size_t total;

    memset(array, 0, sizeof(*array));

    if (capacity > 0) {
        if (SZ_MUL_OV(size, capacity, &total))
            return SQFS_ERROR_OVERFLOW;

        array->data = malloc(total);
        if (array->data == NULL)
            return SQFS_ERROR_ALLOC;
    }

    array->size = size;
    array->count = capacity;
    return 0;
}

int array_init_copy(array_t *array, const array_t *src) {
    int ret = array_init(array, src->size, src->used);
    if (ret != 0)
        return ret;

    memcpy(array->data, src->data, src->used * src->size);
    array->used = src->used;
    return 0;
}

void array_cleanup(array_t *array) {
    free(array->data);
    memset(array, 0, sizeof(*array));
}

int array_append(array_t *array, const void *data) {
    size_t new_sz, new_count;
    void *new_ptr;

    if (array->used == array->count) {
        if (array->count == 0) {
            new_count = 128;
        } else if (SZ_MUL_OV(array->count, 2, &new_count)) {
            return SQFS_ERROR_ALLOC;
        }

        if (SZ_MUL_OV(new_count, array->size, &new_sz))
            return SQFS_ERROR_ALLOC;

        new_ptr = realloc(array->data, new_sz);
        if (new_ptr == NULL)
            return SQFS_ERROR_ALLOC;

        array->data = new_ptr;
        array->count = new_count;
    }

    memcpy((char *)array->data + array->size * array->used, data, array->size);
    array->used += 1;
    return 0;
}

int array_set_capacity(array_t *array, size_t capacity) {
    size_t new_sz, new_count;
    void *new_ptr;

    if (capacity <= array->count)
        return 0;

    if (array->count == 0) {
        new_count = 128;
    } else if (SZ_MUL_OV(array->count, 2, &new_count)) {
        return SQFS_ERROR_ALLOC;
    }

    while (new_count < capacity) {
        if (SZ_MUL_OV(new_count, 2, &new_count))
            return SQFS_ERROR_ALLOC;
    }

    if (SZ_MUL_OV(new_count, array->size, &new_sz))
        return SQFS_ERROR_ALLOC;

    new_ptr = realloc(array->data, new_sz);
    if (new_ptr == NULL)
        return SQFS_ERROR_ALLOC;

    array->data = new_ptr;
    array->count = new_count;
    return 0;
}
