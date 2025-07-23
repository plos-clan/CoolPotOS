#include "frame.h"
#include "boot.h"
#include "hhdm.h"
#include "io.h"
#include "klog.h"
#include "page.h"

FrameAllocator frame_allocator;
uint64_t       memory_size = 0;

void init_frame() {
    struct limine_memmap_response *memory_map = get_memory_map();
    memory_size                               = get_memory_size();

    extern uint64_t reserved_memory;
    extern uint64_t bad_memory;
    extern uint64_t all_memory;

    size_t   bitmap_size    = (memory_size / 4096 + 7) / 8;
    uint64_t bitmap_address = 0;

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        if (region->type == LIMINE_MEMMAP_USABLE && region->length >= bitmap_size) {
            bitmap_address = region->base;
            break;
        }
    }

    if (!bitmap_address) return;

    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_init(bitmap, phys_to_virt(bitmap_address), bitmap_size);

    size_t origin_frames = 0;
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *region = memory_map->entries[i];

        switch (region->type) {
        case LIMINE_MEMMAP_USABLE:
            size_t start_frame  = region->base / 4096;
            size_t frame_count  = region->length / 4096;
            origin_frames      += frame_count;
            bitmap_set_range(bitmap, start_frame, start_frame + frame_count, true);
            all_memory += region->length;
            break;
        case LIMINE_MEMMAP_RESERVED:
        case LIMINE_MEMMAP_ACPI_NVS:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            reserved_memory += region->length;
            all_memory      += region->length;
            break;
        case LIMINE_MEMMAP_BAD_MEMORY:
            bad_memory += region->length;
            all_memory += region->length;
            break;
        }
    }

    size_t bitmap_frame_start = bitmap_address / 4096;
    size_t bitmap_frame_count = (bitmap_size + 4095) / 4096;
    size_t bitmap_frame_end   = bitmap_frame_start + bitmap_frame_count;
    bitmap_set_range(bitmap, bitmap_frame_start, bitmap_frame_end, false);

    frame_allocator.origin_frames = origin_frames;
    frame_allocator.usable_frames = origin_frames - bitmap_frame_count;

    logkf("Available memory: %lld MiB\n", (origin_frames / 256));
}

void free_frame(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_set(bitmap, frame_index, true);
    frame_allocator.usable_frames++;
}

void free_frames_2M(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    for (size_t i = 0; i < 512; i++) {
        bitmap_set(bitmap, frame_index + i, true);
    }
    frame_allocator.usable_frames += 512;
}

void free_frames_1G(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    for (size_t i = 0; i < 262144; i++) {
        bitmap_set(bitmap, frame_index + i, true);
    }
    frame_allocator.usable_frames += 262144;
}

uint64_t alloc_frames(size_t count) {
    Bitmap *bitmap      = &frame_allocator.bitmap;
    size_t  frame_index = bitmap_find_range(bitmap, count, true);

    if (frame_index == (size_t)-1) return 0;
    bitmap_set_range(bitmap, frame_index, frame_index + count, false);
    frame_allocator.usable_frames -= count;

    return frame_index * 4096;
}

uint64_t alloc_frames_2M(size_t count) {
    Bitmap *bitmap         = &frame_allocator.bitmap;
    size_t  frames_per_2mb = 512;
    size_t  total_frames   = count * frames_per_2mb;

    for (size_t i = 0; i < bitmap->length; i += frames_per_2mb) {
        if (i + total_frames > bitmap->length) break;

        if (bitmap_range_all(bitmap, i, i + total_frames, true)) {
            bitmap_set_range(bitmap, i, i + total_frames, false);
            frame_allocator.usable_frames -= total_frames;
            return i * 4096;
        }
    }

    return 0;
}

uint64_t alloc_frames_1G(size_t count) {
    Bitmap *bitmap         = &frame_allocator.bitmap;
    size_t  frames_per_1gb = 262144;
    size_t  total_frames   = count * frames_per_1gb;

    for (size_t i = 0; i < bitmap->length; i += frames_per_1gb) {
        if (i + total_frames > bitmap->length) break;

        if (bitmap_range_all(bitmap, i, i + total_frames, true)) {
            bitmap_set_range(bitmap, i, i + total_frames, false);
            frame_allocator.usable_frames -= total_frames;
            return i * 4096;
        }
    }

    return 0;
}
