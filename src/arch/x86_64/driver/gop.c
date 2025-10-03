#include "gop.h"
#include "boot.h"
#include "cptype.h"
#include "device.h"
#include "errno.h"
#include "heap.h"
#include "hhdm.h"
#include "ioctl.h"
#include "krlibc.h"
#include "page.h"
#include "sprintf.h"
#include "terminal.h"
#include "tty.h"

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

int gop_ioctl(device_t *device, size_t req, void *arg) {
    switch (req) {
    case FBIOGET_FSCREENINFO:
        struct fb_fix_screeninfo *fb_fix = (struct fb_fix_screeninfo *)arg;
        if (fb_fix == NULL) return -EINVAL;
        strcpy(fb_fix->id, "DEFAULT GOP");
        fb_fix->smem_start   = (uint64_t)virt_to_phys((uint64_t)framebuffer->address);
        fb_fix->smem_len     = framebuffer->pitch * framebuffer->height;
        fb_fix->type         = FB_TYPE_PACKED_PIXELS;
        fb_fix->type_aux     = 0;
        fb_fix->visual       = FB_VISUAL_TRUECOLOR;
        fb_fix->xpanstep     = 0;
        fb_fix->ypanstep     = 0;
        fb_fix->ywrapstep    = 0;
        fb_fix->line_length  = framebuffer->pitch;
        fb_fix->mmio_start   = (uint64_t)framebuffer->address;
        fb_fix->mmio_len     = framebuffer->pitch * framebuffer->height;
        fb_fix->capabilities = 0;
        break;
    case FBIOGET_VSCREENINFO:
        struct fb_var_screeninfo *fb_var = (struct fb_var_screeninfo *)arg;
        if (fb_var == NULL) return -EINVAL;
        fb_var->xres         = framebuffer->width;
        fb_var->yres         = framebuffer->height;
        fb_var->xres_virtual = framebuffer->width;
        fb_var->yres_virtual = framebuffer->height;
        fb_var->red          = (struct fb_bitfield){.offset    = framebuffer->red_mask_shift,
                                                    .length    = framebuffer->red_mask_size,
                                                    .msb_right = 0};
        fb_var->green        = (struct fb_bitfield){.offset    = framebuffer->green_mask_shift,
                                                    .length    = framebuffer->green_mask_size,
                                                    .msb_right = 0};
        fb_var->blue         = (struct fb_bitfield){.offset    = framebuffer->blue_mask_shift,
                                                    .length    = framebuffer->blue_mask_size,
                                                    .msb_right = 0};
        fb_var->transp       = (struct fb_bitfield){.offset = 24, .length = 8, .msb_right = 0};

        fb_var->bits_per_pixel = framebuffer->bpp;
        fb_var->grayscale      = 0;
        fb_var->nonstd         = 0;
        fb_var->activate       = 0;                       // idek
        fb_var->height         = framebuffer->height / 4; // VERY approximate
        fb_var->width          = framebuffer->width / 4;  // VERY approximate
        break;
    case FBIOGET_CON2FBMAP:
        struct fb_con2fbmap *con2fb = (struct fb_con2fbmap *)arg;
        if (con2fb == NULL) return -EINVAL;
        if (con2fb->console < 0) { return -EINVAL; }
        if (con2fb->console == 0) {
            con2fb->framebuffer = 0;
        } else {
            return -ENODEV;
        }
        break;
    case TIOCGWINSZ:
        struct winsize *ws = (struct winsize *)arg;
        if (ws == NULL) return -EINVAL;
        ws->ws_col    = get_terminal_col(framebuffer->width);
        ws->ws_row    = get_terminal_row(framebuffer->height);
        ws->ws_xpixel = framebuffer->width;
        ws->ws_ypixel = framebuffer->height;
        break;
    default: return -ENOTTY;
    }
    return EOK;
}

void gop_dev_setup() {
    framebuffers = malloc(sizeof(struct limine_framebuffer *) * MAX_DEIVCE);
    for (uint64_t i = 0; i < get_framebuffer_response()->framebuffer_count; i++) {
        device_t fbdev;
        fbdev.type = DEVICE_FB;
        char name[16];
        sprintf(name, "fb%lu", i);
        strcpy(fbdev.drive_name, name);
        fbdev.flag        = 1;
        fbdev.size        = framebuffer->width * framebuffer->height;
        fbdev.sector_size = 4;
        fbdev.map         = gop_map;
        fbdev.ioctl       = gop_ioctl;
        int id            = regist_device(NULL, fbdev);
        framebuffers[id]  = get_framebuffer_response()->framebuffers[i];
    }
}
