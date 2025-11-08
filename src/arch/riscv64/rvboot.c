#include "boot.h"
#include "driver/fdt.h"
#include "krlibc.h"

extern char        _kernel_start[], _kernel_end[];
uintptr_t          kernel_start = (uintptr_t)_kernel_start;
uintptr_t          kernel_end   = (uintptr_t)_kernel_end;
uint64_t           dtb_ptr_     = 0;
boot_framebuffer_t rv_boot_fb;
boot_memory_map_t  rv_boot_memory_map;

static void setup_framebuffer(boot_framebuffer_t *fb) {
    int chosen_off = fdt_find_node("/chosen");
    if (chosen_off < 0) { return; }

    const char *stdout_path = fdt_get_property_string(chosen_off, "stdout-path");
    if (!stdout_path) { return; }

    char        node_path[256];
    const char *colon    = strchr(stdout_path, ':');
    size_t      path_len = colon ? (size_t)(colon - stdout_path) : strlen(stdout_path);
    if (path_len >= sizeof(node_path)) return;
    memcpy(node_path, stdout_path, path_len);
    node_path[path_len] = '\0';

    int fb_off = fdt_find_node(node_path);
    if (fb_off < 0) { return; }

    const char *compatible = fdt_get_property_string(fb_off, "compatible");
    if (!compatible || strstr(compatible, "simple-framebuffer") == NULL) { return; }

    uint32_t    width, height, stride;
    const char *format;

    if (fdt_get_property_u32(fb_off, "width", &width) < 0) return;
    if (fdt_get_property_u32(fb_off, "height", &height) < 0) return;
    if (fdt_get_property_u32(fb_off, "stride", &stride) < 0) return;
    format = fdt_get_property_string(fb_off, "format");
    if (!format) return;

    uint8_t red_size = 0, red_shift = 0;
    uint8_t green_size = 0, green_shift = 0;
    uint8_t blue_size = 0, blue_shift = 0;
    uint8_t alpha_size = 0, alpha_shift = 0;

    if (strcmp(format, "a8r8g8b8") == 0 || strcmp(format, "x8r8g8b8") == 0) {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
        alpha_size  = 8;
        alpha_shift = 24;
    } else if (strcmp(format, "r5g6b5") == 0) {
        blue_size   = 5;
        blue_shift  = 0;
        green_size  = 6;
        green_shift = 5;
        red_size    = 5;
        red_shift   = 11;
    } else {
        blue_size   = 8;
        blue_shift  = 0;
        green_size  = 8;
        green_shift = 8;
        red_size    = 8;
        red_shift   = 16;
    }

    int         reg_len;
    const void *reg = fdt_get_property(fb_off, "reg", &reg_len);
    if (!reg || reg_len < 16) {}

    uint64_t fb_phys = fdt64_to_cpu(*(uint64_t *)reg);
    // uint64_t fb_size = fdt64_to_cpu(*((uint64_t *)reg + 1));

    fb->address = (uintptr_t)fb_phys;
    fb->width   = (size_t)width;
    fb->height  = (size_t)height;
    fb->pitch   = (size_t)stride;                                   // pitch = stride in bytes
    fb->bpp     = (red_size + green_size + blue_size + alpha_size); // bits per pixel

    fb->red_mask_size    = red_size;
    fb->red_mask_shift   = red_shift;
    fb->green_mask_size  = green_size;
    fb->green_mask_shift = green_shift;
    fb->blue_mask_size   = blue_size;
    fb->blue_mask_shift  = blue_shift;
}

static void setup_memmap(boot_memory_map_t *mmap, uintptr_t kernel_start, uintptr_t kernel_end,
                         const boot_framebuffer_t *fb) {
    mmap->entry_count = 0;

    struct {
        uint64_t base, size;
    } phys_mem[4];
    int phys_mem_count = 0;

    uint32_t *p = (uint32_t *)g_fdt_ctx.dt_struct;
    while (1) {
        uint32_t tag = fdt32_to_cpu(*p++);
        if (tag == FDT_END) break;

        if (tag == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            if (strncmp(name, "memory@", 7) == 0 || strcmp(name, "memory") == 0) {
                p = (uint32_t *)ALIGN_UP((uintptr_t)p + strlen(name) + 1, 4);

                int         node_off = (uint8_t *)(p - 1) - (uint8_t *)g_fdt_ctx.dt_struct - 4;
                int         reg_len;
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
            struct fdt_property *prop = (struct fdt_property *)(p - 1);
            uint32_t             len  = fdt32_to_cpu(prop->len);
            p = (uint32_t *)ALIGN_UP((uintptr_t)p + sizeof(struct fdt_property) + len, 4);
        }
    }

    if (phys_mem_count == 0) { return; }

    typedef struct {
        uint64_t base, size;
        int      type;
    } reserved_region_t;

    reserved_region_t reserved[64];
    int               reserved_count = 0;

    if (kernel_end > kernel_start) {
        reserved[reserved_count].base = kernel_start;
        reserved[reserved_count].size = kernel_end - kernel_start;
        reserved[reserved_count].type = BOOT_MMAP_RESERVED;
        reserved_count++;
    }

    if (fb && fb->address && fb->pitch && fb->height) {
        uint64_t fb_base              = fb->address;
        uint64_t fb_size              = (uint64_t)fb->pitch * fb->height;
        reserved[reserved_count].base = fb_base;
        reserved[reserved_count].size = fb_size;
        reserved[reserved_count].type = BOOT_MMAP_FRAMEBUFFER;
        reserved_count++;
    }

    int resmem_off = fdt_find_node("/reserved-memory");
    if (resmem_off >= 0) {}

    int chosen_off = fdt_find_node("/chosen");
    if (chosen_off >= 0) {
        uint64_t initrd_start = 0, initrd_end = 0;
        fdt_get_property_u64(chosen_off, "linux,initrd-start", &initrd_start);
        fdt_get_property_u64(chosen_off, "linux,initrd-end", &initrd_end);
        if (initrd_end > initrd_start) {
            reserved[reserved_count].base = initrd_start;
            reserved[reserved_count].size = initrd_end - initrd_start;
            reserved[reserved_count].type = BOOT_MMAP_KERNEL_MODULE;
            reserved_count++;
        }
    }

    for (int i = 0; i < phys_mem_count; i++) {
        uint64_t mem_start = phys_mem[i].base;
        uint64_t mem_end   = mem_start + phys_mem[i].size;

        struct {
            uint64_t start, end;
        } usable_chunks[64];
        int usable_count                  = 0;
        usable_chunks[usable_count].start = mem_start;
        usable_chunks[usable_count].end   = mem_end;
        usable_count                      = 1;

        for (int r = 0; r < reserved_count; r++) {
            uint64_t res_start = reserved[r].base;
            uint64_t res_end   = res_start + reserved[r].size;

            for (int u = 0; u < usable_count; u++) {
                uint64_t u_start = usable_chunks[u].start;
                uint64_t u_end   = usable_chunks[u].end;

                if (res_end <= u_start || res_start >= u_end) { continue; }

                usable_chunks[u].end = res_start;

                if (res_end < u_end) {
                    if (usable_count < 64) {
                        memmove(&usable_chunks[u + 2], &usable_chunks[u + 1],
                                (usable_count - u - 1) * sizeof(usable_chunks[0]));
                        usable_chunks[u + 1].start = res_end;
                        usable_chunks[u + 1].end   = u_end;
                        usable_count++;
                    }
                }

                if (usable_chunks[u].start >= usable_chunks[u].end) {
                    memmove(&usable_chunks[u], &usable_chunks[u + 1],
                            (usable_count - u - 1) * sizeof(usable_chunks[0]));
                    usable_count--;
                    u--;
                }
            }
        }

        for (int u = 0; u < usable_count; u++) {
            if (mmap->entry_count >= 8192) break;
            mmap->entries[mmap->entry_count].base   = usable_chunks[u].start;
            mmap->entries[mmap->entry_count].length = usable_chunks[u].end - usable_chunks[u].start;
            mmap->entries[mmap->entry_count].type   = BOOT_MMAP_USABLE;
            mmap->entry_count++;
        }

        for (int r = 0; r < reserved_count; r++) {
            if (reserved[r].base >= mem_start && reserved[r].base + reserved[r].size <= mem_end) {
                if (mmap->entry_count >= 8192) break;
                mmap->entries[mmap->entry_count].base   = reserved[r].base;
                mmap->entries[mmap->entry_count].length = reserved[r].size;
                mmap->entries[mmap->entry_count].type   = reserved[r].type;
                mmap->entry_count++;
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

void setup_rvboot(unsigned long hartid, void *dtb_ptr) {
    dtb_ptr_ = (uint64_t)dtb_ptr;
    if (fdt_init() != 0)
        while (true)
            arch_wait_for_interrupt();
    setup_framebuffer(&rv_boot_fb);
    setup_memmap(&rv_boot_memory_map, kernel_start, kernel_end, &rv_boot_fb);
}

uint64_t boot_get_dtb() {
    return dtb_ptr_;
}

char *get_kernel_cmdline() {
    int chosen_off = fdt_find_node("/chosen");
    if (chosen_off < 0) { return NULL; }
    char *cmdline = fdt_get_property_string(chosen_off, "bootargs");
    return cmdline; // DTB 原始指针
}

size_t boot_framebuffer_count() {
    return 1;
}

boot_framebuffer_t *boot_get_framebuffer(size_t index) {
    return &rv_boot_fb;
}

uint64_t boot_get_hhdm_offset() {
    return 0xFFFFFF8000000000UL;
}

boot_memory_map_t *boot_get_memory_map() {
    return &rv_boot_memory_map;
}
