#include "term/terminal.h"
#include "driver/tty.h"
#include "errno.h"
#include "flanterm/flanterm.h"
#include "flanterm/flanterm_backends/fb.h"
#include "mem/heap.h"

void terminal_flush(tty_t *session) {
    flanterm_flush(session->terminal);
}

size_t terminal_write(tty_t *device, const char *buf, size_t count) {
    flanterm_write(device->terminal, buf, count);
    return count;
}

errno_t create_session_terminal(tty_t *session) {
    if (session->device == NULL) return -ENODEV;
    if (session->device->type != TTY_DEVICE_GRAPHI) return -EINVAL;
    struct tty_graphics_    *framebuffer = session->device->private_data;
    struct flanterm_context *fl_context  = flanterm_fb_init(
        NULL, NULL, framebuffer->address, framebuffer->width, framebuffer->height,
        framebuffer->pitch, framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift, framebuffer->blue_mask_size,
        framebuffer->blue_mask_shift, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
        0, 256);
    session->terminal  = fl_context;
    session->ops.flush = terminal_flush;
    session->ops.write = terminal_write;
    session->ops.read  = NULL;
    session->ops.ioctl = NULL;
    return EOK;
}
