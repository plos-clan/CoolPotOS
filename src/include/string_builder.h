#pragma once

#include "types.h"

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} string_builder_t;

string_builder_t *create_string_builder(size_t initial_capacity);
bool string_builder_append(string_builder_t *buf, const char *format, ...);
