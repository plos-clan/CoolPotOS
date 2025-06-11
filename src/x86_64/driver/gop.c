#include "gop.h"
#include "boot.h"
#include "ctype.h"
#include "heap.h"
#include "krlibc.h"
#include "page.h"
#include "sprintf.h"
#include "vdisk.h"

struct limine_framebuffer  *framebuffer  = NULL;
struct limine_framebuffer **framebuffers = NULL;

uint64_t get_screen_width() {
    return framebuffer->width;
}

uint64_t get_screen_height() {
    return framebuffer->height;
}

void init_gop() {
    framebuffer = get_graphics_framebuffer();
    if (framebuffer == NULL) cpu_hlt;
    gop_clear(0xffffff);
}

void gop_clear(uint32_t color) {
    uint64_t stride = framebuffer->pitch / 4;

    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            *((uint32_t *)framebuffer->address + (y * stride + x)) = color;
        }
    }
}

void *gop_map(int drive, void *addr, uint64_t len) {
    if (framebuffers[drive] == NULL) return NULL;
    struct limine_framebuffer *framebuffer1     = framebuffers[drive];
    uint64_t                   framebuffer_size = framebuffer1->pitch * framebuffer1->height;
    if (len > framebuffer_size) { len = framebuffer_size; }
    uint64_t aligned_len = PADDING_UP(len, PAGE_SIZE);
    page_map_range_to(get_current_directory(), (uint64_t)framebuffer1->address, aligned_len,
                      PTE_USER | PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE);
    return (void *)addr;
}

void gop_dev_setup() {
    framebuffers =
        malloc(sizeof(struct limine_framebuffer *) * get_framebuffer_response()->framebuffer_count);
    for (uint64_t i = 0; i < get_framebuffer_response()->framebuffer_count; i++) {
        vdisk fbdev;
        fbdev.type = VDISK_STREAM;
        char name[16];
        sprintf(name, "fb%lu", i);
        strcpy(fbdev.drive_name, name);
        fbdev.flag        = 1;
        fbdev.size        = framebuffer->width * framebuffer->height;
        fbdev.sector_size = 4;
        fbdev.map         = gop_map;
        int id            = regist_vdisk(fbdev);
        framebuffers[id]  = get_framebuffer_response()->framebuffers[i];
    }
}
