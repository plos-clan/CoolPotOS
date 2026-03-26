#include "mem/frame.h"
#include "boot.h"
#include "bootarg.h"
#include "krlibc.h"
#include "mem/bitmap.h"
#include "mem/buddy.h"
#include "mem/page.h"
#include "mem/page_ref.h"
#include "term/klog.h"

static uint64_t physical_memory_offset;

static spin_t frame_op_lock = SPIN_INIT;
static size_t early_last_alloc_pos = 0;
static Bitmap usable_regions;

uint64_t memory_size = 0;

Bitmap *get_usable_regions() {
    return &usable_regions;
}

uint64_t get_memory_size() {
    uint64_t all_memory_size      = 0;
    boot_memory_map_t *memory_map = boot_get_memory_map();

    for (uint64_t i = memory_map->entry_count - 1;; i--) {
        struct boot_memory_map_entry region = memory_map->entries[i];
        if (region.type == BOOT_MMAP_USABLE) {
            all_memory_size = region.base + region.length;
            break;
        }
    }
    return all_memory_size;
}

uint64_t mem_parse_size(const char *s) {
    if (s == NULL) {
        return UINT64_MAX;
    }

    uint8_t *p     = (uint8_t *)s;
    uint64_t value = 0;

    while (isdigit((*p))) {
        int digit = *p - '0';
        value     = value * 10 + digit;
        p++;
    }
    if (*p == '\0') {
        return value;
    }
    char suffix = *p;

    if (suffix >= 'a' && suffix <= 'z') {
        suffix -= ('a' - 'A');
    }

    switch (suffix) {
    case 'K':
        value *= KILO_FACTOR; // 乘以 1024 (2^10)
        break;
    case 'M':
        value *= MEGA_FACTOR; // 乘以 1048576 (2^20)
        break;
    case 'G':
        value *= GIGA_FACTOR; // 乘以 1073741824 (2^30)
        break;
    default:
        return value;
    }
    if (*(p + 1) != '\0') {
        return UINT64_MAX;
    }

    return value;
}

uint64_t alloc_frames_early(size_t count) {
    if (count == 0)
        return UINT64_MAX;

    spin_lock(frame_op_lock);
    Bitmap *bitmap = &usable_regions;
    size_t frame_index =
        bitmap_find_range_from(bitmap, count, true, early_last_alloc_pos);
    if (frame_index == (size_t)-1 || frame_index + count > bitmap->length) {
        spin_unlock(frame_op_lock);
        return UINT64_MAX;
    }
    bitmap_set_range(bitmap, frame_index, frame_index + count, false);
    early_last_alloc_pos = frame_index + count;
    spin_unlock(frame_op_lock);
    return frame_index * PAGE_SIZE;
}

void *early_alloc(size_t size) {
    if (size == 0)
        return NULL;

    size_t aligned_size =
        (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t phys = alloc_frames_early(aligned_size / PAGE_SIZE);
    if (phys == UINT64_MAX)
        return NULL;

    void *ptr = phys_to_virt(phys);
    memset(ptr, 0, aligned_size);
    return ptr;
}

static uintptr_t get_zone_boundary(enum zone_type type) {
    switch (type) {
#if defined(__x86_64__)
    case ZONE_DMA:
        return ZONE_DMA_END;
#endif
    case ZONE_DMA32:
        return ZONE_DMA32_END;
    case ZONE_NORMAL:
        return UINTPTR_MAX;
    default:
        return 0;
    }
}

// 处理单个内存区域，正确处理不连续的可用帧
static void process_memory_region(uintptr_t start, uintptr_t end) {
    // 页对齐
    start = PADDING_UP(start, PAGE_SIZE);
    end = PADDING_DOWN(end, PAGE_SIZE);

    if (start >= end)
        return;

    uintptr_t current = start;

    while (current < end) {
        // 确定当前位置所属的 zone
        enum zone_type type = phys_to_zone_type(current);

        // 找到同一 zone 的边界
        uintptr_t zone_boundary = get_zone_boundary(type);
        uintptr_t zone_end = MIN(zone_boundary, end);

        // 在当前 zone 内查找连续的可用区域
        uintptr_t region_current = current;

        while (region_current < zone_end) {
            size_t frame = region_current / PAGE_SIZE;

            // 跳过不可用的帧
            while (region_current < zone_end &&
                   !bitmap_get(&usable_regions,
                               region_current / PAGE_SIZE)) {
                region_current += PAGE_SIZE;
                               }

            if (region_current >= zone_end)
                break;

            // 找到连续可用区域的起始
            uintptr_t usable_start = region_current;

            // 找到连续可用区域的结束
            while (region_current < zone_end &&
                   bitmap_get(&usable_regions,
                              region_current / PAGE_SIZE)) {
                region_current += PAGE_SIZE;
                              }

            uintptr_t usable_end = region_current;

            // 添加这段连续可用的区域到 buddy 分配器
            if (usable_end > usable_start) {
                add_memory_region(usable_start, usable_end, type);
            }
        }

        current = zone_end;
    }
}

void init_frame() {
    physical_memory_offset = boot_get_hhdm_offset();
    const char *mem_str    = boot_get_cmdline_param("mem");

    if (mem_str == NULL)
        goto Ldefault;
    if (strcmp(mem_str, "default") != 0) {
        memory_size = mem_parse_size(mem_str);
        memory_size =
            memory_size == UINT64_MAX ? get_memory_size() : PADDING_DOWN(memory_size, PAGE_SIZE);
    } else {
    Ldefault:
        memory_size = get_memory_size();
    }

    // 计算 bitmap 大小
    size_t total_frames        = memory_size / PAGE_SIZE;
    size_t bitmap_size         = (total_frames + 7) / 8;
    size_t bitmap_size_aligned = PADDING_UP(bitmap_size, PAGE_SIZE);

    uint64_t bitmap_address = 0;

    // 查找存放 bitmap 的位置
    for (uint64_t i = 0; i < boot_get_memory_map()->entry_count; i++) {
        boot_memory_map_entry_t *region = &boot_get_memory_map()->entries[i];

#if defined(__x86_64__)
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type == BOOT_MMAP_USABLE && region->length >= bitmap_size_aligned) {
            bitmap_address = region->base;
            break;
        }
    }

    if (bitmap_address == 0) {
        // 无法找到足够大的区域存放 bitmap
        logkf("Cannot find memory for frame bitmap");
    }

    // 初始化 bitmap（所有位初始为 0 = 不可用）
    bitmap_init(&usable_regions, phys_to_virt(bitmap_address), bitmap_size);

    // 标记可用区域
    for (uint64_t i = 0; i < boot_get_memory_map()->entry_count; i++) {
        const boot_memory_map_entry_t *region = &boot_get_memory_map()->entries[i];

#ifdef __x86_64__
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type == BOOT_MMAP_USABLE) {
            size_t start_frame = region->base / PAGE_SIZE;
            size_t end_frame   = (region->base + region->length) / PAGE_SIZE;

            if (end_frame > start_frame) {
                bitmap_set_range(&usable_regions, start_frame, end_frame, true);
            }
        }
    }

#if defined(__x86_64__)
    // 保留低 1MB
    size_t low_1M_frames = 0x100000 / PAGE_SIZE;
    bitmap_set_range(&usable_regions, 0, low_1M_frames, false);
#endif

    // 标记 bitmap 自身占用的区域为不可用
    size_t bitmap_frame_start = bitmap_address / PAGE_SIZE;
    size_t bitmap_frame_end   = PADDING_UP(bitmap_address + bitmap_size, PAGE_SIZE) / PAGE_SIZE;
    bitmap_set_range(&usable_regions, bitmap_frame_start, bitmap_frame_end, false);

    page_ref_init();
    buddy_init();

    // 将可用内存添加到 buddy 分配器
    for (uint64_t i = 0; i < boot_get_memory_map()->entry_count; i++) {
        boot_memory_map_entry_t *region = &boot_get_memory_map()->entries[i];

#if defined(__x86_64__)
        if (region->base < 0x100000)
            continue;
#endif

        if (region->type != BOOT_MMAP_USABLE)
            continue;

        uintptr_t addr       = region->base;
        uintptr_t region_end = region->base + region->length;

        // 跳过 bitmap 占用的部分
        if (addr <= bitmap_address && bitmap_address < region_end) {
            // bitmap 在这个区域内
            uintptr_t bitmap_end = PADDING_UP(bitmap_address + bitmap_size, PAGE_SIZE);

            // 处理 bitmap 之前的部分
            if (addr < bitmap_address) {
                process_memory_region(addr, bitmap_address);
            }

            // 处理 bitmap 之后的部分
            if (bitmap_end < region_end) {
                process_memory_region(bitmap_end, region_end);
            }
        } else {
            process_memory_region(addr, region_end);
        }
    }
}

uint64_t get_physical_memory_offset() {
    return physical_memory_offset;
}

void *phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0)
        return NULL;
    return (void *)(phys_addr + physical_memory_offset);
}

uint64_t virt_to_phys(void *virt_addr) {
    if (virt_addr == 0)
        return 0;
    return (uint64_t)(virt_addr - physical_memory_offset);
}

void *driver_phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0)
        return NULL;
    return (void *)(phys_addr + DRIVER_AREA_MEM);
}

uint64_t driver_virt_to_phys(void *virt_addr) {
    if (virt_addr == 0)
        return 0;
    return (uint64_t)(virt_addr - DRIVER_AREA_MEM);
}
