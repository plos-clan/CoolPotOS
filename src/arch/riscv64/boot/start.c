#include "boot.h"
#include "krlibc.h"
#include "lib/libfdt/libfdt.h"

extern uint8_t _bss_start[], _bss_end[];

extern boot_memory_map_t  opensbi_memory_map;
extern boot_framebuffer_t opensbi_fb;
extern char              *kernel_cmdline;
void                     *opensbi_dtb_vaddr;

static bool fdt_getprop_u64(const void *fdt, int node, const char *name, int idx, uint64_t *out) {
    int            len;
    const fdt64_t *p = fdt_getprop(fdt, node, name, &len);
    if (!p || len < ((idx + 1) * sizeof(uint64_t))) return false;
    *out = fdt64_to_cpu(p[idx]);
    return true;
}

static void setup_framebuffer(boot_framebuffer_t *fb) {
    memset(fb, 0, sizeof(*fb));

    void *fdt    = opensbi_dtb_vaddr;
    int   fb_off = fdt_path_offset(fdt, "/framebuffer");

    // 如果找不到 /framebuffer，尝试按 compatible 查找 simple-framebuffer
    if (fb_off < 0) {
        int node = -1;
        while (1) {
            node = fdt_node_offset_by_compatible(fdt, node, "simple-framebuffer");
            if (node < 0) break;
            fb_off = node;
            break;
        }
    }

    if (fb_off < 0) goto no_fb;

    // 获取基本属性
    const fdt32_t *prop32;
    uint32_t       width = 0, height = 0, stride = 0;

    prop32 = fdt_getprop(fdt, fb_off, "width", NULL);
    if (!prop32) goto no_fb;
    width = fdt32_to_cpu(*prop32);

    prop32 = fdt_getprop(fdt, fb_off, "height", NULL);
    if (!prop32) goto no_fb;
    height = fdt32_to_cpu(*prop32);

    prop32 = fdt_getprop(fdt, fb_off, "stride", NULL);
    if (prop32)
        stride = fdt32_to_cpu(*prop32);
    else
        stride = width * 4; // 默认 32bpp

    const char *format = fdt_getprop(fdt, fb_off, "format", NULL);
    if (!format) format = "x8r8g8b8";

    // 解析像素格式
    uint8_t red_size = 0, red_shift = 0;
    uint8_t green_size = 0, green_shift = 0;
    uint8_t blue_size = 0, blue_shift = 0;
    uint8_t alpha_size = 0, alpha_shift = 0;
    uint8_t bpp = 0;

    if (strcmp(format, "a8r8g8b8") == 0) {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
        alpha_size  = 8;
        alpha_shift = 24;
        bpp         = 32;
    } else if (strcmp(format, "x8r8g8b8") == 0) {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
        bpp         = 32;
    } else if (strcmp(format, "a8b8g8r8") == 0) {
        red_size    = 8;
        red_shift   = 0;
        green_size  = 8;
        green_shift = 8;
        blue_size   = 8;
        blue_shift  = 16;
        alpha_size  = 8;
        alpha_shift = 24;
        bpp         = 32;
    } else if (strcmp(format, "x8b8g8r8") == 0) {
        red_size    = 8;
        red_shift   = 0;
        green_size  = 8;
        green_shift = 8;
        blue_size   = 8;
        blue_shift  = 16;
        bpp         = 32;
    } else if (strcmp(format, "r5g6b5") == 0) {
        blue_size   = 5;
        blue_shift  = 0;
        green_size  = 6;
        green_shift = 5;
        red_size    = 5;
        red_shift   = 11;
        bpp         = 16;
    } else if (strcmp(format, "r8g8b8") == 0) {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
        bpp         = 24;
    } else if (strcmp(format, "b8g8r8") == 0) {
        red_size    = 8;
        red_shift   = 0;
        green_size  = 8;
        green_shift = 8;
        blue_size   = 8;
        blue_shift  = 16;
        bpp         = 24;
    } else {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
        bpp         = 32;
    }

    // 获取 reg 属性
    int            reg_len;
    const fdt64_t *reg = fdt_getprop(fdt, fb_off, "reg", &reg_len);
    if (!reg || reg_len < 16) goto no_fb;

    uint64_t fb_phys = fdt64_to_cpu(reg[0]);
    uint64_t fb_size = fdt64_to_cpu(reg[1]);

    // 验证 framebuffer 大小是否足够
    size_t required_size = (size_t)stride * height;
    if (fb_size < required_size) goto no_fb;

    // 填充 framebuffer 信息
    fb->address = (uintptr_t)fb_phys;
    fb->width   = width;
    fb->height  = height;
    fb->pitch   = stride;
    fb->bpp     = bpp;

    fb->red_mask_size    = red_size;
    fb->red_mask_shift   = red_shift;
    fb->green_mask_size  = green_size;
    fb->green_mask_shift = green_shift;
    fb->blue_mask_size   = blue_size;
    fb->blue_mask_shift  = blue_shift;

    return;

no_fb:
    memset(fb, 0, sizeof(*fb));
}

static void setup_memmap(boot_memory_map_t *mmap, uintptr_t kernel_start, uintptr_t kernel_end,
                         const boot_framebuffer_t *fb) {
    /* 清零并初始化计数 */
    mmap->entry_count = 0;

    /* 1) 收集物理 memory 节点（最多 8 个以防万一） */
    struct {
        uint64_t base, size;
    } phys_mem[8];
    int phys_mem_count = 0;

    /* 遍历所有节点，挑出 name == "memory" 或 name startswith "memory@" 的节点 */
    int offset = -1;
    while ((offset = fdt_next_node(opensbi_dtb_vaddr, offset, NULL)) >= 0) {
        const char *name = fdt_get_name(opensbi_dtb_vaddr, offset, NULL);
        if (!name) continue;
        if (strncmp(name, "memory@", 7) == 0 || strcmp(name, "memory") == 0) {
            int            reg_len;
            const fdt64_t *reg =
                (const fdt64_t *)fdt_getprop(opensbi_dtb_vaddr, offset, "reg", &reg_len);
            if (reg && reg_len >= (int)sizeof(uint64_t) * 2 &&
                phys_mem_count < (int)(sizeof(phys_mem) / sizeof(phys_mem[0]))) {
                phys_mem[phys_mem_count].base = fdt64_to_cpu(reg[0]);
                phys_mem[phys_mem_count].size = fdt64_to_cpu(reg[1]);
                phys_mem_count++;
            }
        }
    }

    if (phys_mem_count == 0) {
        /* 没有 memory 节点就直接返回（空 mmap） */
        return;
    }

    /* 2) 收集保留区域（reserved list） */
    typedef struct {
        uint64_t base, size;
        int      type; /* 使用你的 enum 常量 */
    } reserved_region_t;

    /* 预留数组大小合理上限 */
    reserved_region_t reserved[256];
    int               reserved_count = 0;

    /* 2.1 内核区域作为 RESERVED（这里把内核标记为可执行与模块区，如果你要 kernel_module 可改） */
    if (kernel_end > kernel_start && reserved_count < (int)sizeof(reserved) / sizeof(reserved[0])) {
        reserved[reserved_count].base = (uint64_t)kernel_start;
        reserved[reserved_count].size = (uint64_t)(kernel_end - kernel_start);
        /* 现代 API 建议使用 EXECUTABLE_AND_MODULES；若需要旧值请改为 BOOT_MMAP_KERNEL_MODULE */
        reserved[reserved_count].type = BOOT_MMAP_EXECUTABLE_AND_MODULES;
        reserved_count++;
    }

    /* 2.2 framebuffer 区域作为 FRAMEBUFFER 类型保留（如果 fb 有效） */
    if (fb && fb->address && fb->pitch && fb->height &&
        reserved_count < (int)sizeof(reserved) / sizeof(reserved[0])) {
        reserved[reserved_count].base = (uint64_t)fb->address;
        reserved[reserved_count].size = (uint64_t)fb->pitch * fb->height;
        reserved[reserved_count].type = BOOT_MMAP_FRAMEBUFFER; /* 特殊类型 */
        reserved_count++;
    }

    /* 2.3 /reserved-memory 子节点（如果有），每个子节点的 reg 属性作为 reserved */
    int resmem_off = fdt_path_offset(opensbi_dtb_vaddr, "/reserved-memory");
    if (resmem_off >= 0) {
        int child;
        /* 遍历 reserved-memory 的子节点 */
        for (child = fdt_first_subnode(opensbi_dtb_vaddr, resmem_off); child >= 0;
             child = fdt_next_subnode(opensbi_dtb_vaddr, child)) {
            int            reg_len;
            const fdt64_t *reg =
                (const fdt64_t *)fdt_getprop(opensbi_dtb_vaddr, child, "reg", &reg_len);
            if (!reg || reg_len < (int)sizeof(uint64_t) * 2) continue;
            if (reserved_count >= (int)sizeof(reserved) / sizeof(reserved[0])) break;
            reserved[reserved_count].base = fdt64_to_cpu(reg[0]);
            reserved[reserved_count].size = fdt64_to_cpu(reg[1]);
            reserved[reserved_count].type = BOOT_MMAP_RESERVED;
            reserved_count++;
        }
    }

    /* 2.4 /chosen 下的 initrd（linux,initrd-start/end） */
    int chosen_off = fdt_path_offset(opensbi_dtb_vaddr, "/chosen");
    if (chosen_off >= 0 && reserved_count < (int)sizeof(reserved) / sizeof(reserved[0])) {
        uint64_t initrd_start = 0, initrd_end = 0;
        bool     has_start =
            fdt_getprop_u64(opensbi_dtb_vaddr, chosen_off, "linux,initrd-start", 0, &initrd_start);
        bool has_end =
            fdt_getprop_u64(opensbi_dtb_vaddr, chosen_off, "linux,initrd-end", 0, &initrd_end);
        if (has_start && has_end && initrd_end > initrd_start) {
            reserved[reserved_count].base = initrd_start;
            reserved[reserved_count].size = initrd_end - initrd_start;
            reserved[reserved_count].type = BOOT_MMAP_BOOTLOADER_RECLAIMABLE;
            reserved_count++;
        }
    }

    /* 3) 以每个 phys_mem 为范围计算 usable chunk，排除 reserved 区域 */
    for (int m = 0; m < phys_mem_count; m++) {
        uint64_t mem_start = phys_mem[m].base;
        uint64_t mem_end   = mem_start + phys_mem[m].size;

        /* 简单可用块列表（上限合理） */
        struct {
            uint64_t start, end;
        } usable[256];
        int usable_count = 1;
        usable[0].start  = mem_start;
        usable[0].end    = mem_end;

        /* 对每个 reserved 区域，从 usable 中剔除 */
        for (int r = 0; r < reserved_count; r++) {
            uint64_t rs = reserved[r].base;
            uint64_t re = reserved[r].base + reserved[r].size;

            /* 若没有交集，跳过 */
            if (re <= mem_start || rs >= mem_end) continue;

            /* 检查并在 usable 列表中裁剪 */
            for (int u = 0; u < usable_count; u++) {
                uint64_t us = usable[u].start;
                uint64_t ue = usable[u].end;

                /* 无交集 */
                if (re <= us || rs >= ue) continue;

                if (rs > us && re < ue) {
                    if (usable_count + 1 < (int)sizeof(usable) / sizeof(usable[0])) {
                        /* 把后半段插入 */
                        memmove(&usable[u + 2], &usable[u + 1],
                                (usable_count - u - 1) * sizeof(usable[0]));
                        usable[u + 1].start = re;
                        usable[u + 1].end   = ue;
                        usable[u].end       = rs;
                        usable_count++;
                        u++;
                    } else {
                        /* 空间不足：尽可能裁剪前段 */
                        usable[u].end = rs;
                    }
                } else if (rs <= us && re < ue) {
                    /* reserved 覆盖开头：保留后半段 */
                    usable[u].start = re;
                } else if (rs > us && re >= ue) {
                    /* reserved 覆盖尾部：保留前半段 */
                    usable[u].end = rs;
                } else {
                    /* reserved 覆盖整个块：移除该块 */
                    memmove(&usable[u], &usable[u + 1], (usable_count - u - 1) * sizeof(usable[0]));
                    usable_count--;
                    u--; /* 重新检查当前位置 */
                }
            }
        }

        for (int u = 0; u < usable_count; u++) {
            uint64_t s = usable[u].start;
            uint64_t e = usable[u].end;
            if (s >= e) continue;
            if (mmap->entry_count >= (int)(sizeof(mmap->entries) / sizeof(mmap->entries[0]))) break;
            mmap->entries[mmap->entry_count].base   = (uintptr_t)s;
            mmap->entries[mmap->entry_count].length = (size_t)(e - s);
            mmap->entries[mmap->entry_count].type   = BOOT_MMAP_USABLE;
            mmap->entry_count++;
        }
    }

    for (int r = 0; r < reserved_count; r++) {
        for (int m = 0; m < phys_mem_count; m++) {
            uint64_t ms = phys_mem[m].base;
            uint64_t me = ms + phys_mem[m].size;
            uint64_t rb = reserved[r].base;
            uint64_t re = reserved[r].base + reserved[r].size;
            if (rb >= ms && re <= me) {
                if (mmap->entry_count >= (int)(sizeof(mmap->entries) / sizeof(mmap->entries[0])))
                    break;
                mmap->entries[mmap->entry_count].base   = (uintptr_t)rb;
                mmap->entries[mmap->entry_count].length = (size_t)(re - rb);
                /* 使用 reserved 中保存的 type（例如 BOOT_MMAP_FRAMEBUFFER / BOOT_MMAP_RESERVED / BOOT_MMAP_BOOTLOADER_RECLAIMABLE / BOOT_MMAP_EXECUTABLE_AND_MODULES） */
                mmap->entries[mmap->entry_count].type = (int)reserved[r].type;
                mmap->entry_count++;
                break;
            }
        }
    }

    for (size_t i = 0; i < mmap->entry_count; i++) {
        for (size_t j = i + 1; j < mmap->entry_count; j++) {
            if (mmap->entries[i].base > mmap->entries[j].base) {
                boot_memory_map_entry_t tmp = mmap->entries[i];
                mmap->entries[i]            = mmap->entries[j];
                mmap->entries[j]            = tmp;
            }
        }
    }
}

static const char *fdt_kernel_cmdline(void *fdt) {
    int chosen_off = fdt_path_offset(fdt, "/chosen");
    if (chosen_off < 0) return NULL;

    int         len      = 0;
    const char *bootargs = fdt_getprop(fdt, chosen_off, "bootargs", &len);
    if (!bootargs || len <= 0) return NULL;

    return bootargs;
}

extern void init_early_paging();

uint64_t bsp_hart_id;

USED void opensbi_c_start(uint64_t boot_hart_id, uintptr_t dtb_ptr) {
    memset(&_bss_start, 0, (uint8_t *)&_bss_end - (uint8_t *)&_bss_start);

    bsp_hart_id = boot_hart_id;

    struct fdt_header *header;

    header = (struct fdt_header *)dtb_ptr;
    if (!header) { return; }

    /* 检查魔数 */
    if (fdt_check_header((void *)dtb_ptr) != 0) { return; }

    /* 获取DTB总大小 */
    fdt_header_size((void *)dtb_ptr);

    opensbi_dtb_vaddr = (void *)dtb_ptr;
    setup_framebuffer(&opensbi_fb);
    setup_memmap(&opensbi_memory_map, 0x80000000, 0x81000000, &opensbi_fb);
    kernel_cmdline = (char*)fdt_kernel_cmdline(opensbi_dtb_vaddr);
    init_early_paging();
    __asm__ volatile("j kmain");
}
