#include "string_builder.h"
#include "lib/sprintf.h"
#include "mem/heap.h"
#include "types/limits.h"

string_builder_t *create_string_builder(size_t initial_capacity) {
    if (initial_capacity == 0)
        initial_capacity = 1;
    string_builder_t *buf = malloc(sizeof(string_builder_t));
    if (!buf)
        return NULL;

    buf->data = calloc(1, initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->size = 0;
    buf->capacity = initial_capacity;

    return buf;
}

bool string_builder_append(string_builder_t *buf, const char *format, ...) {
    if (!buf || !format)
        return false;

    va_list args;
    int needed;

    // 第一次调用计算所需空间
    va_start(args, format);
    needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0)
        return false;

    // 防止整数溢出
    if (needed > SIZE_MAX - buf->size - 1)
        return false;
    size_t required = buf->size + (size_t)needed + 1;

    // 确保缓冲区足够大
    if (required > buf->capacity) {
        size_t new_capacity = buf->capacity ? buf->capacity : 16;

        // 确保至少增长到 required
        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2) {
                // 溢出处理
                new_capacity = SIZE_MAX;
                if (new_capacity < required)
                    return false;
                break;
            }
            new_capacity *= 2;
        }

        char *new_data = realloc(buf->data, new_capacity);
        if (!new_data)
            return false;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    // 实际写入
    size_t avail = buf->capacity - buf->size;
    if (avail > (size_t)INT_MAX)
        avail = (size_t)INT_MAX;

    va_start(args, format);
    int written = vsnprintf(buf->data + buf->size, (int)avail, format, args);
    va_end(args);

    if (written < 0)
        return false;
    if ((size_t)written >= avail)
        return false;

    // 更新大小（写入的字符数不会超过 needed）
    buf->size += (size_t)written;
    buf->data[buf->size] = '\0'; // 确保 null 终止

    return true;
}
