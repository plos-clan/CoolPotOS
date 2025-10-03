#pragma once

#include "cptype.h"
#include "krlibc.h"

typedef struct {
    uint8_t *buffer;
    size_t   length;
} Bitmap;

void bitmap_init(Bitmap *bitmap, uint8_t *buffer, size_t size);

bool bitmap_get(const Bitmap *bitmap, size_t index);

void bitmap_set(Bitmap *bitmap, size_t index, bool value);

void bitmap_set_range(Bitmap *bitmap, size_t start, size_t end, bool value);

size_t bitmap_find_range(const Bitmap *bitmap, size_t length, bool value);

bool bitmap_range_all(const Bitmap *bitmap, size_t start, size_t end, bool value);

void bitmap_free_frames(uint64_t addr, size_t count);

void bitmap_free_frame(uint64_t addr);

void bitmap_free_frames_2M(uint64_t addr);

void bitmap_free_frames_1G(uint64_t addr);

uint64_t bitmap_alloc_frames(size_t count);

uint64_t bitmap_alloc_frames_2M(size_t count);

uint64_t bitmap_alloc_frames_1G(size_t count);

void init_frame_bitmap(uint64_t memory_size);
