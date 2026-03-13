#include "term/terminal.h"
#include "driver/tty.h"
#include "errno.h"
#include "flanterm/flanterm.h"
#include "flanterm/flanterm_backends/fb.h"

void terminal_flush(tty_t *session) {
    spin_lock(session->lock);
    flanterm_flush(session->terminal);
    spin_unlock(session->lock);
}

size_t terminal_write(tty_t *device, const char *buf, size_t offset, size_t count) {
    spin_lock(device->lock);
    flanterm_write(device->terminal, buf, count);
    spin_unlock(device->lock);
    return count;
}

void terminal_cols_rows(tty_t *session, size_t *cols, size_t *rows) {
    struct flanterm_context *fl_context = session->terminal;

    if (cols != NULL)
        *cols = 80;
    if (rows != NULL)
        *rows = 25;

    if (fl_context != NULL)
        flanterm_get_dimensions(fl_context, cols, rows);
}

void terminal_width_height(tty_t *session, size_t *width, size_t *height) {
    tty_device_t *device = session->device;

    if (width != NULL)
        *width = 640;
    if (height != NULL)
        *height = 400;

    if (device->type == TTY_DEVICE_GRAPHI) {
        struct tty_graphics_ *graphics = device->private_data;
        if (width != NULL)
            *width = graphics->width;
        if (height != NULL)
            *height = graphics->height;
    }
}

errno_t create_session_terminal(tty_t *session) {
    if (session->device == NULL)
        return -ENODEV;
    if (session->device->type != TTY_DEVICE_GRAPHI)
        return -EINVAL;
    struct tty_graphics_ *framebuffer   = session->device->private_data;
    struct flanterm_context *fl_context = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer->address,
        framebuffer->width,
        framebuffer->height,
        framebuffer->pitch,
        framebuffer->red_mask_size,
        framebuffer->red_mask_shift,
        framebuffer->green_mask_size,
        framebuffer->green_mask_shift,
        framebuffer->blue_mask_size,
        framebuffer->blue_mask_shift,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        0,
        0,
        256
    );
    session->terminal  = fl_context;
    session->ops.flush = terminal_flush;
    session->ops.write = terminal_write;
    return EOK;
}
