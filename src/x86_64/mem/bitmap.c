#include "bitmap.h"
#include "boot.h"
#include "frame.h"
#include "hhdm.h"
#include "klog.h"

extern FrameAllocator frame_allocator;
extern uint64_t       memory_size;

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

void bitmap_free_frames(uint64_t addr, size_t count) {
    if (addr == 0 || count == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_set_range(bitmap, frame_index, frame_index + count, true);
}

void bitmap_free_frame(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_set(bitmap, frame_index, true);
}

void bitmap_free_frames_2M(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    for (size_t i = 0; i < 512; i++) {
        bitmap_set(bitmap, frame_index + i, true);
    }
}

void bitmap_free_frames_1G(uint64_t addr) {
    if (addr == 0) return;
    size_t frame_index = addr / 4096;
    if (frame_index == 0) return;
    Bitmap *bitmap = &frame_allocator.bitmap;
    for (size_t i = 0; i < 262144; i++) {
        bitmap_set(bitmap, frame_index + i, true);
    }
}

uint64_t bitmap_alloc_frames(size_t count) {
    Bitmap *bitmap      = &frame_allocator.bitmap;
    size_t  frame_index = bitmap_find_range(bitmap, count, true);

    if (frame_index == (size_t)-1) return 0;
    bitmap_set_range(bitmap, frame_index, frame_index + count, false);
    frame_allocator.usable_frames -= count;

    return frame_index * 4096;
}

uint64_t bitmap_alloc_frames_2M(size_t count) {
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

uint64_t bitmap_alloc_frames_1G(size_t count) {
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

void init_frame_bitmap(uint64_t memory_size) {
    struct limine_memmap_response *memory_map = get_memory_map();

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

    logkf("Available memory: %lld MiB bitmap_sz: %llu\n", (origin_frames / 256), bitmap_size);
}
