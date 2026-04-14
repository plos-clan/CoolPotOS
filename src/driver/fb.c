#include "driver/fb.h"
#include "boot.h"
#include "driver/ioctl.h"
#include "driver/tty.h"
#include "errno.h"
#include "fs/devtmpfs.h"
#include "fs/sysfs.h"
#include "krlibc.h"

#include <mem/page.h>

static uint64_t fb_phys_addr(const boot_framebuffer_t *fb) {
    return arch_virt_to_phys(fb->address);
}

static uint32_t fb_alpha_offset(const boot_framebuffer_t *fb) {
    uint32_t max_end = 0;

    uint32_t red_end = (uint32_t)fb->red_mask_shift + (uint32_t)fb->red_mask_size;
    if (red_end > max_end) {
        max_end = red_end;
    }

    uint32_t green_end = (uint32_t)fb->green_mask_shift + (uint32_t)fb->green_mask_size;
    if (green_end > max_end) {
        max_end = green_end;
    }

    uint32_t blue_end = (uint32_t)fb->blue_mask_shift + (uint32_t)fb->blue_mask_size;
    if (blue_end > max_end) {
        max_end = blue_end;
    }

    return max_end;
}

static uint32_t fb_alpha_length(const boot_framebuffer_t *fb) {
    uint32_t used_bits =
        (uint32_t)fb->red_mask_size + (uint32_t)fb->green_mask_size + (uint32_t)fb->blue_mask_size;
    return fb->bpp > used_bits ? (uint32_t)(fb->bpp - used_bits) : 0;
}

static void
fb_fill_fix_screeninfo(const boot_framebuffer_t *framebuffer, struct fb_fix_screeninfo *fb_fix) {
    memset(fb_fix, 0, sizeof(*fb_fix));
    snprintf(fb_fix->id, sizeof(fb_fix->id), "%s", FB_DEVICE_NAME);

    fb_fix->smem_start  = fb_phys_addr(framebuffer);
    fb_fix->smem_len    = framebuffer->pitch * framebuffer->height;
    fb_fix->type        = FB_TYPE_PACKED_PIXELS;
    fb_fix->visual      = FB_VISUAL_TRUECOLOR;
    fb_fix->line_length = framebuffer->pitch;
    fb_fix->mmio_start  = fb_fix->smem_start;
    fb_fix->mmio_len    = fb_fix->smem_len;
}

static void
fb_fill_var_screeninfo(const boot_framebuffer_t *framebuffer, struct fb_var_screeninfo *fb_var) {
    memset(fb_var, 0, sizeof(*fb_var));

    fb_var->xres           = framebuffer->width;
    fb_var->yres           = framebuffer->height;
    fb_var->xres_virtual   = framebuffer->width;
    fb_var->yres_virtual   = framebuffer->height;
    fb_var->bits_per_pixel = framebuffer->bpp;

    fb_var->red = (struct fb_bitfield){
        .offset    = framebuffer->red_mask_shift,
        .length    = framebuffer->red_mask_size,
        .msb_right = 0,
    };
    fb_var->green = (struct fb_bitfield){
        .offset    = framebuffer->green_mask_shift,
        .length    = framebuffer->green_mask_size,
        .msb_right = 0,
    };
    fb_var->blue = (struct fb_bitfield){
        .offset    = framebuffer->blue_mask_shift,
        .length    = framebuffer->blue_mask_size,
        .msb_right = 0,
    };

    const uint32_t alpha_length = fb_alpha_length(framebuffer);
    fb_var->transp              = (struct fb_bitfield){
                     .offset    = alpha_length == 0 ? 0 : fb_alpha_offset(framebuffer),
                     .length    = alpha_length,
                     .msb_right = 0,
    };

    fb_var->height = (uint32_t)-1;
    fb_var->width  = (uint32_t)-1;
}

static size_t fb_dev_read(const void *data, void *buf, const size_t offset, size_t len) {
    const boot_framebuffer_t *fb = data;
    const size_t fb_size         = fb->pitch * fb->height;
    if (offset >= fb_size) {
        return 0;
    }
    if (offset + len > fb_size) {
        len = fb_size - offset;
    }
    memcpy(buf, (char *)fb->address + offset, len);
    return len;
}

static size_t fb_dev_write(const void *data, const void *buf, const size_t offset, size_t len) {
    const boot_framebuffer_t *fb = data;
    size_t fb_size               = fb->pitch * fb->height;
    if (offset >= fb_size) {
        return 0;
    }
    if (offset + len > fb_size) {
        len = fb_size - offset;
    }
    memcpy((char *)fb->address + offset, buf, len);
    return len;
}

static errno_t fb_dev_ioctl(void *data, size_t cmd, void *arg) {
    const boot_framebuffer_t *framebuffer = data;

    cmd = cmd & 0xFFFFFFFF;

    switch (cmd) {
    case FBIOGET_FSCREENINFO:;
        if (arg == NULL) {
            return (errno_t)-EINVAL;
        }
        struct fb_fix_screeninfo *fb_fix = (struct fb_fix_screeninfo *)arg;
        fb_fill_fix_screeninfo(framebuffer, fb_fix);
        return 0;
    case FBIOGET_VSCREENINFO:;
        if (arg == NULL) {
            return (errno_t)-EINVAL;
        }
        struct fb_var_screeninfo *fb_var = (struct fb_var_screeninfo *)arg;
        fb_fill_var_screeninfo(framebuffer, fb_var);
        return 0;
    case FBIOGETCMAP:;
    case FBIOPUTCMAP:;
    case FBIOPAN_DISPLAY:;
    case FBIOBLANK:;
        return 0;
    case TIOCGWINSZ:;
        if (arg == NULL) {
            return (errno_t)-EINVAL;
        }
        struct winsize *win = (struct winsize *)arg;
        win->ws_col         = framebuffer->width / 8;
        win->ws_row         = framebuffer->height / 16;

        win->ws_xpixel = (uint16_t)framebuffer->width;
        win->ws_ypixel = (uint16_t)framebuffer->height;

        return 0;
    case FBIOPUT_VSCREENINFO:;
        if (arg == NULL) {
            return (errno_t)-EINVAL;
        }
        fb_fill_var_screeninfo(framebuffer, (struct fb_var_screeninfo *)arg);
        return 0;
    default:
        return (errno_t)-ENOTTY;
    }
}

static errno_t fb_dev_poll(void *data, const size_t events) {
    int revents = 0;
    if (events & 0x1) {
        revents |= 0x1; // POLLIN
    }
    if (events & 0x4) {
        revents |= 0x4; // POLLOUT
    }
    return revents;
}

static size_t fb_dev_size(void *data) {
    const boot_framebuffer_t *fb = data;
    return fb->pitch * fb->height;
}

static void *
fb_dev_map(void *data, void *addr, const size_t offset, size_t size, size_t prot, size_t flags) {
    const boot_framebuffer_t *framebuffer = (boot_framebuffer_t *)data;
    const size_t fb_size                  = framebuffer->pitch * framebuffer->height;

    if (offset >= fb_size) {
        return NULL;
    }

    if (size == 0 || offset + size > fb_size) {
        size = fb_size - offset;
    }

    const uint64_t fb_addr = fb_phys_addr(framebuffer) + offset;

    const uint64_t page_flags =

#if defined(__x86_64__) || defined(__amd64__)
        PTE_USER | PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE;
#else
        0; // TODO
#endif

    page_map_range(get_current_directory(), (uint64_t)addr, fb_addr, size, page_flags);
    return addr;
}

static vfs_node_t fb_sysfs_ensure_real_dir(void) {
    if (sysfs_get_root() == NULL || sysfs_get_devices_root() == NULL) {
        return NULL;
    }

    vfs_node_t virtual_root = sysfs_ensure_dir(sysfs_get_devices_root(), "virtual");
    if (virtual_root == NULL) {
        return NULL;
    }

    vfs_node_t graphics_root = sysfs_ensure_dir(virtual_root, "graphics");
    if (graphics_root == NULL) {
        return NULL;
    }

    return sysfs_ensure_dir(graphics_root, "fb0");
}

static void fb_sysfs_publish(void) {
    if (boot_framebuffer_count() == 0 || sysfs_get_root() == NULL
        || sysfs_get_class_root() == NULL) {
        return;
    }

    boot_framebuffer_t *fb = boot_get_framebuffer(0);
    if (fb == NULL) {
        return;
    }

    vfs_node_t real_dir = fb_sysfs_ensure_real_dir();
    if (real_dir == NULL) {
        return;
    }

    vfs_node_t device_dir = sysfs_ensure_dir(real_dir, "device");

    vfs_node_t class_graphics = sysfs_ensure_dir(sysfs_get_class_root(), "graphics");
    if (class_graphics != NULL && vfs_open_nofollow("/sys/class/graphics/fb0") == NULL) {
        sysfs_child_append_symlink_node(class_graphics, "fb0", real_dir);
    }

    if (vfs_open_nofollow("/sys/devices/virtual/graphics/fb0/subsystem") == NULL) {
        sysfs_child_append_symlink(real_dir, "subsystem", "/sys/class/graphics");
    }

    if (device_dir != NULL
        && vfs_open_nofollow("/sys/devices/virtual/graphics/fb0/device/subsystem") == NULL) {
        sysfs_child_append_symlink(device_dir, "subsystem", "/sys/class/graphics");
    }

    char mode[32];
    snprintf(mode, sizeof(mode), "U:%ux%up-%u\n", fb->width, fb->height, fb->bpp);
    sysfs_create_file(real_dir, "name", FB_DEVICE_NAME "\n");
    sysfs_create_file(real_dir, "mode", mode);
    sysfs_create_file(real_dir, "modes", mode);

    char bits_per_pixel[16];
    snprintf(bits_per_pixel, sizeof(bits_per_pixel), "%u\n", fb->bpp);
    sysfs_create_file(real_dir, "bits_per_pixel", bits_per_pixel);

    char virtual_size[32];
    snprintf(virtual_size, sizeof(virtual_size), "%ux%u\n", fb->width, fb->height);
    sysfs_create_file(real_dir, "virtual_size", virtual_size);

    char stride[32];
    snprintf(stride, sizeof(stride), "%u\n", fb->pitch);
    sysfs_create_file(real_dir, "stride", stride);

    sysfs_create_file(real_dir, "blank", "0\n");
    sysfs_create_file(real_dir, "rotate", "0\n");
    sysfs_create_file(real_dir, "dev", "29:0\n");

    char uevent[128];
    snprintf(
        uevent,
        sizeof(uevent),
        "MAJOR=%u\nMINOR=%u\nDEVNAME=fb0\nSUBSYSTEM=graphics\n",
        FB_MAJOR,
        0U
    );
    sysfs_create_file(real_dir, "uevent", uevent);
    if (device_dir != NULL) {
        sysfs_create_file(device_dir, "uevent", uevent);
    }

    char *real_path = vfs_get_fullpath(real_dir);
    if (real_path != NULL) {
        if (vfs_open_nofollow("/sys/dev/char/29:0") == NULL) {
            sysfs_regist_dev('c', FB_MAJOR, 0, real_path, "fb0", uevent);
        }
        free(real_path);
    }
}

void fb_sysfs_populate(void) {
    fb_sysfs_publish();
}

void fb_setup(const vfs_node_t dev_root) {
    if (boot_framebuffer_count() == 0) {
        return;
    }
    boot_framebuffer_t *fb = boot_get_framebuffer(0);
    if (fb == NULL || dev_root == NULL) {
        return;
    }

    const uint64_t dev_number = (uint64_t)FB_MAJOR << 8 | 0;
    create_device_node(
        dev_root,
        "fb0",
        device_stream,
        fb,
        dev_number,
        fb_dev_ioctl,
        (vfs_read_t)fb_dev_read,
        (vfs_write_t)fb_dev_write,
        fb_dev_poll,
        fb_dev_map,
        fb_dev_size,
        MKDEV(29, 0)
    );

    fb_sysfs_publish();
}
