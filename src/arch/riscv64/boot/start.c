#include "boot.h"
#include "driver/fdt.h"
#include "krlibc.h"

uintptr_t smp_entry = 0;

extern uint8_t _bss_start[], _bss_end[];

extern boot_memory_map_t opensbi_memory_map;
extern boot_framebuffer_t opensbi_fb;

static void setup_framebuffer(boot_framebuffer_t *fb) {
    int chosen_off = fdt_find_node("/chosen");
    if (chosen_off < 0) {
        return;
    }

    const char *stdout_path =
        fdt_get_property_string(chosen_off, "stdout-path");
    if (!stdout_path) {
        return;
    }

    char node_path[256];
    const char *colon = strchr(stdout_path, ':');
    size_t path_len =
        colon ? (size_t)(colon - stdout_path) : strlen(stdout_path);
    if (path_len >= sizeof(node_path))
        return;
    memcpy(node_path, stdout_path, path_len);
    node_path[path_len] = '\0';

    int fb_off = fdt_find_node(node_path);
    if (fb_off < 0) {
        return;
    }

    const char *compatible = fdt_get_property_string(fb_off, "compatible");
    if (!compatible || strstr(compatible, "simple-framebuffer") == NULL) {
        return;
    }

    uint32_t width, height, stride;
    const char *format;

    if (fdt_get_property_u32(fb_off, "width", &width) < 0)
        return;
    if (fdt_get_property_u32(fb_off, "height", &height) < 0)
        return;
    if (fdt_get_property_u32(fb_off, "stride", &stride) < 0)
        return;
    format = fdt_get_property_string(fb_off, "format");
    if (!format)
        return;

    uint8_t red_size = 0, red_shift = 0;
    uint8_t green_size = 0, green_shift = 0;
    uint8_t blue_size = 0, blue_shift = 0;
    uint8_t alpha_size = 0, alpha_shift = 0;

    if (strcmp(format, "a8r8g8b8") == 0 || strcmp(format, "x8r8g8b8") == 0) {
        blue_size = 8;
        blue_shift = 0;
        green_size = 8;
        green_shift = 8;
        red_size = 8;
        red_shift = 16;
        alpha_size = 8;
        alpha_shift = 24;
    } else if (strcmp(format, "r5g6b5") == 0) {
        blue_size = 5;
        blue_shift = 0;
        green_size = 6;
        green_shift = 5;
        red_size = 5;
        red_shift = 11;
    } else {
        blue_size = 8;
        blue_shift = 0;
        green_size = 8;
        green_shift = 8;
        red_size = 8;
        red_shift = 16;
    }

    int reg_len;
    const void *reg = fdt_get_property(fb_off, "reg", &reg_len);
    if (!reg || reg_len < 16) {
    }

    uint64_t fb_phys = fdt64_to_cpu(*(uint64_t *)reg);
    // uint64_t fb_size = fdt64_to_cpu(*((uint64_t *)reg + 1));

    fb->address = (uintptr_t)fb_phys;
    fb->width = (size_t)width;
    fb->height = (size_t)height;
    fb->pitch = (size_t)stride; // pitch = stride in bytes
    fb->bpp =
        (red_size + green_size + blue_size + alpha_size); // bits per pixel

    fb->red_mask_size = red_size;
    fb->red_mask_shift = red_shift;
    fb->green_mask_size = green_size;
    fb->green_mask_shift = green_shift;
    fb->blue_mask_size = blue_size;
    fb->blue_mask_shift = blue_shift;
}

static void setup_memmap(boot_memory_map_t *mmap, uintptr_t kernel_start,
                         uintptr_t kernel_end, const boot_framebuffer_t *fb) {
    mmap->entry_count = 0;

    struct {
        uint64_t base, size;
    } phys_mem[4];
    int phys_mem_count = 0;

    uint32_t *p = (uint32_t *)g_fdt_ctx.dt_struct;
    while (1) {
        uint32_t tag = fdt32_to_cpu(*p++);
        if (tag == FDT_END)
            break;

        if (tag == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            if (strncmp(name, "memory@", 7) == 0 ||
                strcmp(name, "memory") == 0) {
                int node_off =
                    ((uint8_t *)p - 4) - (uint8_t *)g_fdt_ctx.dt_struct;

                p = (uint32_t *)ALIGN_UP((uintptr_t)p + strlen(name) + 1, 4);

                int reg_len;
                const void *reg = fdt_get_property(node_off, "reg", &reg_len);
                if (reg && reg_len >= 16) {
                    uint64_t base = fdt64_to_cpu(*(uint64_t *)reg);
                    uint64_t size = fdt64_to_cpu(*((uint64_t *)reg + 1));
                    if (phys_mem_count < 4) {
                        phys_mem[phys_mem_count].base = base;
                        phys_mem[phys_mem_count].size = size;
                        phys_mem_count++;
                    }
                }
                continue;
            }
            p = (uint32_t *)ALIGN_UP((uintptr_t)p + strlen(name) + 1, 4);
        } else if (tag == FDT_PROP) {
            // 修复：p 已经指向 fdt_property，不需要 -1
            struct fdt_property *prop = (struct fdt_property *)p;
            uint32_t len = fdt32_to_cpu(prop->len);
            p = (uint32_t *)ALIGN_UP(
                (uintptr_t)p + sizeof(struct fdt_property) + len, 4);
        }
    }

    if (phys_mem_count == 0) {
        return;
    }

    typedef struct {
        uint64_t base, size;
        int type;
    } reserved_region_t;

    reserved_region_t reserved[1024];
    int reserved_count = 0;

    // 首先添加内核区域到保留区域
    if (kernel_end > kernel_start) {
        reserved[reserved_count].base = kernel_start;
        reserved[reserved_count].size = kernel_end - kernel_start;
        reserved[reserved_count].type = BOOT_MMAP_RESERVED;
        reserved_count++;
    }

    if (fb && fb->address && fb->pitch && fb->height) {
        uint64_t fb_base = fb->address;
        uint64_t fb_size = (uint64_t)fb->pitch * fb->height;
        reserved[reserved_count].base = fb_base;
        reserved[reserved_count].size = fb_size;
        reserved[reserved_count].type = BOOT_MMAP_RESERVED;
        reserved_count++;
    }

    int resmem_off = fdt_find_node("/reserved-memory");
    if (resmem_off >= 0) {
        // 处理预留内存区域
    }

    int chosen_off = fdt_find_node("/chosen");
    if (chosen_off >= 0) {
        uint64_t initrd_start = 0, initrd_end = 0;
        fdt_get_property_u64(chosen_off, "linux,initrd-start", &initrd_start);
        fdt_get_property_u64(chosen_off, "linux,initrd-end", &initrd_end);
        if (initrd_end > initrd_start) {
            reserved[reserved_count].base = initrd_start;
            reserved[reserved_count].size = initrd_end - initrd_start;
            reserved[reserved_count].type = BOOT_MMAP_RESERVED;
            reserved_count++;
        }
    }

    for (int i = 0; i < phys_mem_count; i++) {
        uint64_t mem_start = phys_mem[i].base;
        uint64_t mem_end = mem_start + phys_mem[i].size;

        // 初始化可用块列表，开始时只有一个块：整个物理内存区域
        struct {
            uint64_t start, end;
        } usable_chunks[1024];
        int usable_count = 0;
        usable_chunks[usable_count].start = mem_start;
        usable_chunks[usable_count].end = mem_end;
        usable_count = 1;

        // 处理保留区域与可用块的重叠
        for (int r = 0; r < reserved_count; r++) {
            uint64_t res_start = reserved[r].base;
            uint64_t res_end = res_start + reserved[r].size;

            // 检查保留区域是否与当前物理内存区域有重叠
            if (res_end <= mem_start || res_start >= mem_end) {
                continue; // 没有重叠，跳过
            }

            // 在当前所有可用块中剔除这个保留区域
            for (int u = 0; u < usable_count; u++) {
                uint64_t u_start = usable_chunks[u].start;
                uint64_t u_end = usable_chunks[u].end;

                // 检查保留区域是否与当前可用块重叠
                if (res_end <= u_start || res_start >= u_end) {
                    continue; // 没有重叠
                }

                // 处理重叠情况
                if (res_start > u_start) {
                    // 保留区域在可用块中间或末尾，保留前面的部分
                    usable_chunks[u].end = res_start;

                    if (res_end < u_end) {
                        // 保留区域在可用块中间，还需要添加后面的部分
                        if (usable_count < 1024) {
                            // 将后面的块插入到u+1位置
                            memmove(&usable_chunks[u + 1], &usable_chunks[u],
                                    (usable_count - u) *
                                        sizeof(usable_chunks[0]));
                            usable_chunks[u + 1].start = res_end;
                            usable_chunks[u + 1].end = u_end;
                            usable_count++;
                            u++; // 跳过新插入的块
                        }
                    }
                } else if (res_end < u_end) {
                    // 保留区域在可用块开头，保留后面的部分
                    usable_chunks[u].start = res_end;
                } else {
                    // 保留区域完全覆盖可用块，移除这个可用块
                    memmove(&usable_chunks[u], &usable_chunks[u + 1],
                            (usable_count - u - 1) * sizeof(usable_chunks[0]));
                    usable_count--;
                    u--; // 重新检查当前位置
                }
            }
        }

        // 添加处理后的可用块到内存映射
        for (int u = 0; u < usable_count; u++) {
            if (usable_chunks[u].start < usable_chunks[u].end) { // 确保块有效
                if (mmap->entry_count >=
                    (sizeof(mmap->entries) / sizeof(mmap->entries[0])))
                    break;
                mmap->entries[mmap->entry_count].base = usable_chunks[u].start;
                mmap->entries[mmap->entry_count].length =
                    usable_chunks[u].end - usable_chunks[u].start;
                mmap->entries[mmap->entry_count].type = BOOT_MMAP_USABLE;
                mmap->entry_count++;
            }
        }
    }

    // 修复：添加所有保留区域到内存映射（确保内核区域等被正确标记为RESERVED）
    for (int r = 0; r < reserved_count; r++) {
        if (mmap->entry_count >=
            (sizeof(mmap->entries) / sizeof(mmap->entries[0])))
            break;

        // 检查保留区域是否在某个物理内存区域内
        for (int i = 0; i < phys_mem_count; i++) {
            uint64_t mem_start = phys_mem[i].base;
            uint64_t mem_end = mem_start + phys_mem[i].size;

            if (reserved[r].base >= mem_start &&
                reserved[r].base + reserved[r].size <= mem_end) {
                mmap->entries[mmap->entry_count].base = reserved[r].base;
                mmap->entries[mmap->entry_count].length = reserved[r].size;
                mmap->entries[mmap->entry_count].type = reserved[r].type;
                mmap->entry_count++;
                break;
            }
        }
    }

    // 按地址排序
    for (size_t i = 0; i < mmap->entry_count; i++) {
        for (size_t j = i + 1; j < mmap->entry_count; j++) {
            if (mmap->entries[i].base > mmap->entries[j].base) {
                boot_memory_map_entry_t tmp = mmap->entries[i];
                mmap->entries[i] = mmap->entries[j];
                mmap->entries[j] = tmp;
            }
        }
    }
}

extern void init_early_paging();

uint64_t bsp_hart_id = UINT64_MAX;

uintptr_t opensbi_dtb_vaddr;

USED void opensbi_c_start(uint64_t boot_hart_id, uintptr_t dtb_ptr) {
    if (bsp_hart_id == UINT64_MAX)
        bsp_hart_id = boot_hart_id;

    if (boot_hart_id != bsp_hart_id) {
        while (!smp_entry) {
            arch_pause();
        }

        ((void (*)(void))smp_entry)();
    } else {
        memset(&_bss_start, 0, (uint8_t *)&_bss_end - (uint8_t *)&_bss_start);
    }

    struct fdt_header *header;

    header = (struct fdt_header *)dtb_ptr;
    if (!header) {
        return;
    }

    /* 检查魔数 */
    if (fdt32_to_cpu(header->magic) != FDT_MAGIC) {
        return;
    }

    /* 获取DTB总大小 */
    g_fdt_ctx.dtb_base = (void *)header;
    if (!g_fdt_ctx.dtb_base) {
        return;
    }

    /* 设置各个部分的指针 */
    g_fdt_ctx.header = (struct fdt_header *)g_fdt_ctx.dtb_base;
    g_fdt_ctx.dt_struct =
        (uint8_t *)g_fdt_ctx.dtb_base + fdt32_to_cpu(header->off_dt_struct);
    g_fdt_ctx.dt_strings =
        (char *)g_fdt_ctx.dtb_base + fdt32_to_cpu(header->off_dt_strings);
    g_fdt_ctx.rsv_map =
        (struct fdt_reserve_entry *)((uint8_t *)g_fdt_ctx.dtb_base +
                                     fdt32_to_cpu(header->off_mem_rsvmap));

    setup_framebuffer(&opensbi_fb);
    setup_memmap(&opensbi_memory_map, 0x80000000, 0x81000000, &opensbi_fb);

    opensbi_dtb_vaddr = dtb_ptr;

    init_early_paging();

    __asm__ volatile("j kmain");
}