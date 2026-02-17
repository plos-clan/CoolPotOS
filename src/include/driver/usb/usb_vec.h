#pragma once

#include "mem/alloc/alloc.h"
#include "types.h"

#define USB_VEC_DEFINE(type, name)                                                                 \
    typedef struct name {                                                                          \
        type *data;                                                                                \
        size_t len;                                                                                \
        size_t cap;                                                                                \
    } name;                                                                                        \
    static inline void name##_init(name *vec) {                                                    \
        vec->data = NULL;                                                                          \
        vec->len = 0;                                                                              \
        vec->cap = 0;                                                                              \
    }                                                                                              \
    static inline void name##_free(name *vec) {                                                    \
        if (vec->data) {                                                                           \
            free(vec->data);                                                                       \
        }                                                                                          \
        vec->data = NULL;                                                                          \
        vec->len = 0;                                                                              \
        vec->cap = 0;                                                                              \
    }                                                                                              \
    static inline void name##_clear(name *vec) {                                                   \
        vec->len = 0;                                                                              \
    }                                                                                              \
    static inline bool name##_reserve(name *vec, size_t new_cap) {                                 \
        if (new_cap <= vec->cap) {                                                                 \
            return true;                                                                           \
        }                                                                                          \
        type *new_data = (type *)realloc(vec->data, new_cap * sizeof(type));                       \
        if (!new_data) {                                                                           \
            return false;                                                                          \
        }                                                                                          \
        vec->data = new_data;                                                                      \
        vec->cap = new_cap;                                                                        \
        return true;                                                                               \
    }                                                                                              \
    static inline bool name##_push(name *vec, type value) {                                        \
        if (vec->len == vec->cap) {                                                                \
            size_t new_cap = vec->cap ? vec->cap * 2 : 4;                                          \
            if (!name##_reserve(vec, new_cap)) {                                                   \
                return false;                                                                      \
            }                                                                                      \
        }                                                                                          \
        vec->data[vec->len++] = value;                                                             \
        return true;                                                                               \
    }                                                                                              \
    static inline type *name##_get(name *vec, size_t index) {                                      \
        if (index >= vec->len) {                                                                   \
            return NULL;                                                                           \
        }                                                                                          \
        return &vec->data[index];                                                                  \
    }                                                                                              \
    static inline type *name##_last(name *vec) {                                                   \
        if (vec->len == 0) {                                                                       \
            return NULL;                                                                           \
        }                                                                                          \
        return &vec->data[vec->len - 1];                                                           \
    }                                                                                              \
    static inline type *name##_try_get(name *vec, size_t index) {                                  \
        if (index >= vec->len) {                                                                   \
            return NULL;                                                                           \
        }                                                                                          \
        return &vec->data[index];                                                                  \
    }
