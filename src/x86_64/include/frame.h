#pragma once

#include "bitmap.h"
#include "ctype.h"

typedef struct {
    Bitmap bitmap;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

void init_frame();

void     free_frame(uint64_t addr);
void     free_frames_2M(uint64_t addr);
void     free_frames_1G(uint64_t addr);
uint64_t alloc_frames(size_t count);
uint64_t alloc_frames_2M(size_t count);
uint64_t alloc_frames_1G(size_t count);
