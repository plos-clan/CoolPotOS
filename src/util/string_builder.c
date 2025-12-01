#include "string_builder.h"
#include "lib/sprintf.h"
#include "mem/heap.h"

string_builder_t *create_string_builder(size_t initial_capacity) {
    string_builder_t *buf = malloc(sizeof(string_builder_t));
    if (!buf) return NULL;

    buf->data = malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->size     = 0;
    buf->capacity = initial_capacity;
    buf->data[0]  = '\0';

    return buf;
}

bool string_builder_append(string_builder_t *buf, const char *format, ...) {
    va_list args;
    int     needed;

    // 计算需要的空间
    va_start(args, format);
    needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0) return false;

    // 检查是否需要扩展缓冲区
    size_t required = buf->size + needed + 1;
    if (required > buf->capacity) {
        size_t new_capacity = buf->capacity;
        while (new_capacity < required) {
            new_capacity *= 2;
        }

        char *new_data = realloc(buf->data, new_capacity);
        if (!new_data) return false;

        buf->data     = new_data;
        buf->capacity = new_capacity;
    }

    // 添加新内容
    va_start(args, format);
    int written = vsnprintf(buf->data + buf->size, buf->capacity - buf->size, format, args);
    va_end(args);

    if (written > 0) { buf->size += written; }

    return written >= 0;
}
