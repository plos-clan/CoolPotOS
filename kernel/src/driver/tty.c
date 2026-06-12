#include "driver/tty.h"
#include "boot.h"
#include "bootarg.h"
#include "mem/slub.h"
#include "errno.h"
#include "driver/device.h"
#include "term/terminal.h"

static struct llist_header tty_device_list;
static tty_t *kernel_session = NULL; // 内核会话

tty_device_t *alloc_tty_device(enum tty_device_type type) {
    tty_device_t *device = calloc(1, sizeof(tty_device_t));
    device->type         = type;
    llist_init_head(&device->node);
    return device;
}

uint64_t register_tty_device(tty_device_t *device) {
    if (device->private_data == NULL)
        return -EINVAL;
    llist_append(&tty_device_list, &device->node);
    return EOK;
}

uint64_t delete_tty_device(tty_device_t *device) {
    if (device == NULL)
        return -EINVAL;
    free(device->private_data);
    llist_delete(&device->node);
    free(device);
    return EOK;
}

tty_device_t *get_tty_device(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    tty_device_t *pos = NULL;
    tty_device_t *n   = NULL;
    llist_for_each(pos, n, &tty_device_list, node) {
        if (strcmp(pos->name, name) == 0) {
            return pos;
        }
    }
    return NULL;
}

tty_t *get_kernel_session() {
    return kernel_session;
}

int tty_ioctl(void *dev, int cmd, void *args) {
    tty_t *tty = dev;
    return tty->ops.ioctl(tty, cmd, (uint64_t)args);
}

int tty_poll(void *dev, int events) {
    tty_t *tty = dev;
    return tty->ops.poll(tty, events);
}

int tty_read(void *dev, void *buf, uint64_t offset, size_t size) {
    tty_t *tty = dev;
    (void)offset;
    return tty->ops.read(tty, buf, size);
}

int tty_write(void *dev, const void *buf, uint64_t offset, size_t size) {
    tty_t *tty = dev;
    (void)offset;
    return tty->ops.write(tty, buf, size);
}

static void init_framebuffer_tty() {
    for (size_t i = 0; i < boot_framebuffer_count(); i++) {
        const boot_framebuffer_t *framebuffer = boot_get_framebuffer(i);
        tty_device_t *fb_device               = alloc_tty_device(TTY_DEVICE_GRAPHI);
        struct tty_graphics_ *graphics        = malloc(sizeof(struct tty_graphics_));

        graphics->address = (void *)framebuffer->address;
        graphics->width   = framebuffer->width;
        graphics->height  = framebuffer->height;
        graphics->bpp     = framebuffer->bpp;
        graphics->pitch   = framebuffer->pitch;

        graphics->blue_mask_shift  = framebuffer->blue_mask_shift;
        graphics->red_mask_shift   = framebuffer->red_mask_shift;
        graphics->green_mask_shift = framebuffer->green_mask_shift;
        graphics->blue_mask_size   = framebuffer->blue_mask_size;
        graphics->red_mask_size    = framebuffer->red_mask_size;
        graphics->green_mask_size  = framebuffer->green_mask_size;

        fb_device->private_data = graphics;
        char name[32];
        sprintf(name, "tty%lu", i);
        strcpy(fb_device->name, name);
        register_tty_device(fb_device);
    }
}

void tty_init() {
    llist_init_head(&tty_device_list);
    kernel_session = calloc(sizeof(tty_t), 1);
    init_framebuffer_tty();

    const char *console = boot_get_cmdline_param("console");
    if (console == NULL) {
        console = "tty0";
    }
    tty_device_t *device   = get_tty_device(console);
    kernel_session->device = device;
    if (device->type == TTY_DEVICE_GRAPHI) {
        create_session_terminal(kernel_session);
    }

    device_install(
        DEV_CHAR,
        DEV_TTY,
        kernel_session,
        "tty0",
        0,
        NULL,
        NULL,
        tty_ioctl,
        tty_poll,
        tty_read,
        tty_write,
        NULL
    );
    device_install(
        DEV_CHAR,
        DEV_TTY,
        kernel_session,
        "tty1",
        0,
        NULL,
        NULL,
        tty_ioctl,
        tty_poll,
        tty_read,
        tty_write,
        NULL
    );
}
