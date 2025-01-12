#include "ctype.h"
#include "bitmap.h"

typedef struct {
    Bitmap bitmap;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

void init_frame();

uint64_t alloc_frames(size_t count);
