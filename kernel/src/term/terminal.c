#include "term/terminal.h"
#include "driver/tty.h"
#include "errno.h"
#include "lock.h"
#include "fs/fcntl.h"
#include "mem/page.h"
#define FLANTERM_IN_FLANTERM
#include "../lib/flanterm/flanterm.h"
#include "../lib/flanterm/flanterm_backends/fb.h"
#include "../lib/flanterm/flanterm_private.h"

static spin_t terminal_write_lock = SPIN_INIT;

static void terminal_flush(tty_t *session) {
    spin_lock(terminal_write_lock);
    flanterm_flush(session->terminal);
    spin_unlock(terminal_write_lock);
}

static size_t terminal_write(tty_t *device, const char *buf, size_t count) {
    spin_lock(terminal_write_lock);
    if (device->vtmode.mode != VT_PROCESS && device->terminal) {
        flanterm_write(device->terminal, buf, count);
    }
    spin_unlock(terminal_write_lock);
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

int terminal_ioctl(tty_t *device, uint32_t cmd, uint64_t arg) {
    struct flanterm_context *ft_ctx    = device->terminal;
    struct flanterm_fb_context *fb_ctx = device->terminal;
    switch (cmd) {
    case TIOCGWINSZ: {
        struct winsize ws = {
            .ws_xpixel = fb_ctx->width,
            .ws_ypixel = fb_ctx->height,
            .ws_col    = ft_ctx->cols,
            .ws_row    = ft_ctx->rows,
        };
        if (!arg || copy_to_user((void *)arg, &ws, sizeof(ws)))
            return -EFAULT;
        return 0;
    }
    case TIOCSCTTY:
        return 0;
    case TIOCGPGRP: {
        int pid = device->pgid;
        if (!arg || copy_to_user((void *)arg, &pid, sizeof(pid)))
            return -EFAULT;
        return 0;
    }
    case TIOCSPGRP: {
        int pid = 0;
        if (!arg || copy_from_user(&pid, (void *)arg, sizeof(pid)))
            return -EFAULT;
        device->pgid = pid;
        return 0;
    }
    case TCGETS:
        if (!arg || copy_to_user((void *)arg, &device->term, sizeof(termios))) {
            return -EFAULT;
        }
        return 0;
    case TCGETS2: {
        struct termios2 t2 = { 0 };
        memcpy(&t2.c_iflag, &device->term.c_iflag, sizeof(uint32_t));
        memcpy(&t2.c_oflag, &device->term.c_oflag, sizeof(uint32_t));
        memcpy(&t2.c_cflag, &device->term.c_cflag, sizeof(uint32_t));
        memcpy(&t2.c_lflag, &device->term.c_lflag, sizeof(uint32_t));
        t2.c_line = device->term.c_line;
        memcpy(t2.c_cc, device->term.c_cc, sizeof(t2.c_cc));
        t2.c_ispeed = 0; // Not supported
        t2.c_ospeed = 0; // Not supported
        if (!arg || copy_to_user((void *)arg, &t2, sizeof(struct termios2)))
            return -EFAULT;
        return 0;
    }
    case TCSETS:
        if (!arg || copy_from_user(&device->term, (void *)arg, sizeof(termios))) {
            return -EFAULT;
        }
        return 0;
    case TCSETS2: {
        struct termios2 t2_set;
        if (!arg || copy_from_user(&t2_set, (void *)arg, sizeof(struct termios2)))
            return -EFAULT;
        memcpy(&device->term.c_iflag, &t2_set.c_iflag, sizeof(uint32_t));
        memcpy(&device->term.c_oflag, &t2_set.c_oflag, sizeof(uint32_t));
        memcpy(&device->term.c_cflag, &t2_set.c_cflag, sizeof(uint32_t));
        memcpy(&device->term.c_lflag, &t2_set.c_lflag, sizeof(uint32_t));
        device->term.c_line = t2_set.c_line;
        memcpy(device->term.c_cc, t2_set.c_cc, sizeof(t2_set.c_cc));
        // Ignore ispeed and ospeed as they are not supported
        return 0;
    }
    case TCSETSW:
        if (!arg || copy_from_user(&device->term, (void *)arg, sizeof(termios))) {
            return -EFAULT;
        }
        return 0;
    case TIOCSWINSZ:
        return 0;
    case KDGETMODE: {
        int mode = device->tty_mode;
        if (!arg || copy_to_user((void *)arg, &mode, sizeof(mode)))
            return -EFAULT;
        return 0;
    }
    case KDSETMODE:
        device->tty_mode = arg;
        return 0;
    case KDGKBMODE: {
        int kbmode = device->tty_kbmode;
        if (!arg || copy_to_user((void *)arg, &kbmode, sizeof(kbmode)))
            return -EFAULT;
        return 0;
    }
    case KDSKBMODE:
        device->tty_kbmode = arg;
        return 0;
    case VT_SETMODE:
        if (!arg || copy_from_user(&device->vtmode, (void *)arg, sizeof(vt_mode)))
            return -EFAULT;
        return 0;
    case VT_GETMODE:
        if (!arg || copy_to_user((void *)arg, &device->vtmode, sizeof(vt_mode)))
            return -EFAULT;
        return 0;
    case VT_ACTIVATE:
        return 0;
    case VT_WAITACTIVE:
        return 0;
    case VT_GETSTATE: {
        struct vt_state state = {
            .v_active = 1,
            .v_state  = 0,
        };
        if (!arg || copy_to_user((void *)arg, &state, sizeof(state)))
            return -EFAULT;
        return 0;
    }
    case VT_OPENQRY: {
        int query = 1;
        if (!arg || copy_to_user((void *)arg, &query, sizeof(query)))
            return -EFAULT;
        return 0;
    }
    case TIOCNOTTY:
        return 0;
    case TCSETSF:
        if (!arg || copy_from_user(&device->term, (void *)arg, sizeof(termios)))
            return -EFAULT;
        return 0;
    case TCFLSH:
        // tty_input_flush(device);
        return 0;
    case TIOCNXCL:
        return 0;
    default:
        return -ENOTTY;
    }
}

static void termios_init(termios *termios) {
    termios->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    termios->c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
    termios->c_oflag = OPOST;
    termios->c_cflag = CS8 | CREAD | CLOCAL;
    for (int i = 16; i < NCCS; i++) {
        termios->c_cc[i] = 0;
    }
    termios->c_line         = 0;
    termios->c_cc[VINTR]    = 3;    // Ctrl-C
    termios->c_cc[VQUIT]    = 28;   // Ctrl-Q
    termios->c_cc[VERASE]   = 0x7F; // DEL
    termios->c_cc[VKILL]    = 21;   // Ctrl-U
    termios->c_cc[VEOF]     = 4;    // Ctrl-D
    termios->c_cc[VTIME]    = 0;    // No timer
    termios->c_cc[VMIN]     = 1;    // Return each byte
    termios->c_cc[VSTART]   = 17;   // Ctrl-Q
    termios->c_cc[VSTOP]    = 19;   // Ctrl-S
    termios->c_cc[VSUSP]    = 26;   // Ctrl-Z
    termios->c_cc[VREPRINT] = 18;   // Ctrl-R
    termios->c_cc[VDISCARD] = 15;   // Ctrl-O
    termios->c_cc[VWERASE]  = 23;   // Ctrl-W
    termios->c_cc[VLNEXT]   = 22;
}

int create_session_terminal(tty_t *session) {
    if (session->device == NULL) {
        return -ENODEV;
    }
    if (session->device->type != TTY_DEVICE_GRAPHI) {
        return -EINVAL;
    }
    const struct tty_graphics_ *framebuffer = session->device->private_data;
    uint32_t background_color               = 0x050505;
    struct flanterm_context *fl_context     = flanterm_fb_init(
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
        &background_color,
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

    session->tty_mode   = KD_TEXT;
    session->tty_kbmode = K_XLATE;
    termios_init(&session->term);

    session->terminal  = fl_context;
    session->ops.flush = terminal_flush;
    session->ops.write = terminal_write;
    return EOK;
}
