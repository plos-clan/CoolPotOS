#include "driver/fb.h"
#include "boot.h"
#include "driver/ioctl.h"
#include "driver/tty.h"
#include "errno.h"
#include "fs/devtmpfs.h"
#include "krlibc.h"

#include <mem/page.h>

static size_t fb_dev_read(void *data, void *buf, size_t offset, size_t len) {
    boot_framebuffer_t *fb = (boot_framebuffer_t *)data;
    size_t fb_size         = fb->pitch * fb->height;
    if (offset >= fb_size)
        return 0;
    if (offset + len > fb_size)
        len = fb_size - offset;
    memcpy(buf, (char *)fb->address + offset, len);
    return len;
}

static size_t fb_dev_write(void *data, const void *buf, size_t offset, size_t len) {
    boot_framebuffer_t *fb = (boot_framebuffer_t *)data;
    size_t fb_size         = fb->pitch * fb->height;
    if (offset >= fb_size)
        return 0;
    if (offset + len > fb_size)
        len = fb_size - offset;
    memcpy((char *)fb->address + offset, buf, len);
    return len;
}

static errno_t fb_dev_ioctl(void *data, size_t cmd, void *arg) {
    boot_framebuffer_t *framebuffer = (boot_framebuffer_t *)data;

    cmd = cmd & 0xFFFFFFFF;

    switch (cmd) {
    case FBIOGET_FSCREENINFO:;
        struct fb_fix_screeninfo *fb_fix = (struct fb_fix_screeninfo *)arg;
        memcpy(fb_fix->id, "CPOS-FBDEV", 10);
        fb_fix->smem_start   = arch_virt_to_phys(framebuffer->address);
        fb_fix->smem_len     = framebuffer->pitch * framebuffer->height;
        fb_fix->type         = FB_TYPE_PACKED_PIXELS;
        fb_fix->type_aux     = 0;
        fb_fix->visual       = FB_VISUAL_TRUECOLOR;
        fb_fix->xpanstep     = 0;
        fb_fix->ypanstep     = 0;
        fb_fix->ywrapstep    = 0;
        fb_fix->line_length  = framebuffer->pitch;
        fb_fix->mmio_len     = framebuffer->pitch * framebuffer->height;
        fb_fix->mmio_start   = arch_virt_to_phys(framebuffer->address);
        fb_fix->capabilities = 0;
        return 0;
    case FBIOGET_VSCREENINFO:;
        struct fb_var_screeninfo *fb_var = (struct fb_var_screeninfo *)arg;
        fb_var->xres                     = framebuffer->width;
        fb_var->yres                     = framebuffer->height;

        fb_var->xres_virtual = framebuffer->width;
        fb_var->yres_virtual = framebuffer->height;

        fb_var->red    = (struct fb_bitfield){ .offset    = framebuffer->red_mask_shift,
                                               .length    = framebuffer->red_mask_size,
                                               .msb_right = 0 };
        fb_var->green  = (struct fb_bitfield){ .offset    = framebuffer->green_mask_shift,
                                               .length    = framebuffer->green_mask_size,
                                               .msb_right = 0 };
        fb_var->blue   = (struct fb_bitfield){ .offset    = framebuffer->blue_mask_shift,
                                               .length    = framebuffer->blue_mask_size,
                                               .msb_right = 0 };
        fb_var->transp = (struct fb_bitfield){ .offset = 24, .length = 8, .msb_right = 0 };

        fb_var->bits_per_pixel = framebuffer->bpp;
        fb_var->grayscale      = 0;
        fb_var->nonstd         = 0;
        fb_var->activate       = 0;
        fb_var->height         = framebuffer->height / 4;
        fb_var->width          = framebuffer->width / 4;

        return 0;
    case FBIOPUTCMAP:
        return 0;
    case TIOCGWINSZ:;
        struct winsize *win = (struct winsize *)arg;
        win->ws_col         = framebuffer->width / 8;
        win->ws_row         = framebuffer->height / 16;

        win->ws_xpixel = (uint16_t)framebuffer->width;
        win->ws_ypixel = (uint16_t)framebuffer->height;
        return 0;
    case FBIOPUT_VSCREENINFO:
        return 0;
    default:
        return -ENOTTY;
    }
}

static errno_t fb_dev_poll(void *data, size_t events) {
    int revents = 0;
    if (events & 0x1)
        revents |= 0x1; // POLLIN
    if (events & 0x4)
        revents |= 0x4; // POLLOUT
    return revents;
}

static size_t fb_dev_size(void *data) {
    boot_framebuffer_t *fb = (boot_framebuffer_t *)data;
    return fb->pitch * fb->height;
}

static void *
fb_dev_map(void *data, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    boot_framebuffer_t *framebuffer = (boot_framebuffer_t *)data;
    uint64_t fb_addr                = arch_virt_to_phys(framebuffer->address) + offset;

    uint64_t page_flags =

#if defined(__x86_64__) || defined(__amd64__)
        PTE_USER | PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE;
#else
        0; // TODO
#endif

    page_map_range(
        get_current_directory(), (uint64_t)addr, fb_addr,
        framebuffer->width * framebuffer->height * framebuffer->bpp / 8, page_flags
    );
    return addr;
}

void fb_setup(vfs_node_t dev_root) {
    if (boot_framebuffer_count() == 0)
        return;
    boot_framebuffer_t *fb = boot_get_framebuffer(0);
    if (fb == NULL || dev_root == NULL)
        return;

    uint64_t dev_number = ((uint64_t)FB_MAJOR << 8) | 0;
    create_device_node(
        dev_root, "fb0", device_stream, fb, dev_number, (vfs_ioctl_t)fb_dev_ioctl,
        (vfs_read_t)fb_dev_read, (vfs_write_t)fb_dev_write, (vfs_poll_t)fb_dev_poll,
        (vfs_mapfile_t)fb_dev_map, fb_dev_size
    );
}
