#include "bitmap.h"

void bitmap_init(Bitmap *bitmap, uint8_t *buffer, size_t size) {
    bitmap->buffer = buffer;
    bitmap->length = size * 8;
    memset(buffer, 0, size);
}

bool bitmap_get(const Bitmap *bitmap, size_t index) {
    size_t word_index = index / 8;
    size_t bit_index  = index % 8;
    return (bitmap->buffer[word_index] >> bit_index) & 1;
}

void bitmap_set(Bitmap *bitmap, size_t index, bool value) {
    size_t word_index = index / 8;
    size_t bit_index  = index % 8;
    if (value) {
        bitmap->buffer[word_index] |= ((size_t)1 << bit_index);
    } else {
        bitmap->buffer[word_index] &= ~((size_t)1 << bit_index);
    }
}

void bitmap_set_range(Bitmap *bitmap, size_t start, size_t end, bool value) {
    if (start >= end || start >= bitmap->length) { return; }

    size_t start_word = (start + 7) / 8;
    size_t end_word   = end / 8;

    for (size_t i = start; i < start_word * 8 && i < end; i++) {
        bitmap_set(bitmap, i, value);
    }

    if (start_word > end_word) { return; }

    if (start_word <= end_word) {
        uint8_t fill_value = value ? (uint8_t)-1 : 0;
        for (size_t i = start_word; i < end_word; i++) {
            bitmap->buffer[i] = fill_value;
        }
    }

    for (size_t i = end_word * 8; i < end; i++) {
        bitmap_set(bitmap, i, value);
    }
}

size_t bitmap_find_range(const Bitmap *map, size_t length, bool value) {
    size_t  count = 0, start_index = 0;
    uint8_t full_byte = value ? 0xFF : 0x00;

    size_t byte_len = (map->length + 7) / 8;

    for (size_t byte_idx = 0; byte_idx < byte_len; byte_idx++) {
        uint8_t byte = map->buffer[byte_idx];

        if (byte == full_byte) {
            // 整字节匹配
            if (count == 0) start_index = byte_idx * 8;
            count += 8;
            if (count >= length) return start_index;
        } else if (byte == (uint8_t)~full_byte) {
            // 完全不匹配
            count = 0;
        } else {
            // 部分匹配 → 逐位检查
            for (size_t bit = 0; bit < 8; bit++) {
                size_t idx = byte_idx * 8 + bit;
                if (idx >= map->length) break;

                bool bit_value = (byte >> bit) & 1;
                if (bit_value == value) {
                    if (count == 0) start_index = idx;
                    count++;
                    if (count >= length) return start_index;
                } else {
                    count = 0;
                }
            }
        }
    }
}

bool bitmap_range_all(const Bitmap *bitmap, size_t start, size_t end, bool value) {
    for (size_t i = start; i < end; i++) {
        if (bitmap_get(bitmap, i) != value) return false;
    }
    return true;
}
