#include "driver/tty.h"
#include "bootarg.h"
#include "driver/input_device.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "mem/heap.h"
#include "task/task.h"
#include "term/klog.h"
#include "term/terminal.h"

struct llist_header tty_device_list;
tty_t *kernel_session  = NULL; // 内核会话
tty_t *current_session = NULL; // 当前会话

tty_t *get_kernel_session() {
    return kernel_session;
}

int kernel_getch() {
    int ch;
    const bool int_status = arch_check_interrupt();
    arch_open_interrupt();
    while ((ch = atom_pop(current_session->queue)) == -1) {
        arch_pause();
    }
    if (!int_status) {
        arch_close_interrupt();
    }
    return ch;
}

tty_device_t *alloc_tty_device(enum tty_device_type type) {
    tty_device_t *device = calloc(1, sizeof(tty_device_t));
    device->type         = type;
    llist_init_head(&device->node);
    return device;
}

errno_t register_tty_device(tty_device_t *device) {
    if (device->private_data == NULL)
        return -EINVAL;
    llist_append(&tty_device_list, &device->node);
    return EOK;
}

errno_t delete_tty_device(tty_device_t *device) {
    if (device == NULL)
        return -EINVAL;
    free(device->private_data);
    llist_delete(&device->node);
    free(device);
    return EOK;
}

tty_device_t *get_tty_device(const char *name) {
    if (name == NULL)
        return NULL;
    tty_device_t *pos = NULL;
    tty_device_t *n   = NULL;
    llist_for_each(pos, n, &tty_device_list, node) {
        if (strcmp(pos->name, name) == 0) {
            return pos;
        }
    }
    return NULL;
}

void init_tty() {
    llist_init_head(&tty_device_list);
    kernel_session = malloc(sizeof(tty_t));
}

void tty_event_handle(indev_t *device, intype type, uint64_t code, uint8_t value) {
    if (type == EV_CHAR) {
        char *ascii_code = (char *)code;
        size_t length    = strlen(ascii_code);
        if (length == 0)
            return;
        for (size_t i = 0; i < length; i++) {
            atom_push(current_session->queue, ascii_code[i]);
        }
    }
}

static size_t tty_size_t(tty_t *session) {
    return -1;
}

static void termios_init(termios_t *termios) {
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

static errno_t tty_ioctl(tty_t *session, size_t req, void *arg) {
    switch (req) {
    case TIOCGWINSZ:;
        struct winsize *ws = (struct winsize *)arg;
        if (ws != NULL) {
            size_t col, row, x, y;
            terminal_cols_rows(session, &col, &row);
            terminal_width_height(session, &x, &y);
            ws->ws_col    = col;
            ws->ws_row    = row;
            ws->ws_xpixel = x;
            ws->ws_ypixel = y;
        }
        break;
    case TCGETS:;
        struct termios *termios = (struct termios *)arg;
        termios->c_iflag        = session->termios.c_iflag;
        termios->c_oflag        = session->termios.c_oflag;
        termios->c_cflag        = session->termios.c_cflag;
        termios->c_lflag        = session->termios.c_lflag;

        termios->c_cc[VINTR]    = session->termios.c_cc[VINTR];  // Ctrl-C
        termios->c_cc[VQUIT]    = session->termios.c_cc[VQUIT];  // Ctrl-\

        termios->c_cc[VERASE]   = session->termios.c_cc[VERASE]; // Backspace
        termios->c_cc[VKILL]    = session->termios.c_cc[VKILL];  // Ctrl-U
        termios->c_cc[VEOF]     = session->termios.c_cc[VEOF];   // Ctrl-D
        termios->c_cc[VTIME]    = session->termios.c_cc[VTIME];
        termios->c_cc[VMIN]     = session->termios.c_cc[VMIN];
        termios->c_cc[VSTART]   = session->termios.c_cc[VSTART];   // Ctrl-Q
        termios->c_cc[VSTOP]    = session->termios.c_cc[VSTOP];    // Ctrl-S
        termios->c_cc[VSUSP]    = session->termios.c_cc[VSUSP];    // Ctrl-Z
        termios->c_cc[VREPRINT] = session->termios.c_cc[VREPRINT]; // Ctrl-R
        termios->c_cc[VDISCARD] = session->termios.c_cc[VDISCARD]; // Ctrl-O
        termios->c_cc[VWERASE]  = session->termios.c_cc[VWERASE];  // Ctrl-W
        termios->c_cc[VLNEXT]   = session->termios.c_cc[VLNEXT];   // Ctrl-V

        termios->c_line = session->termios.c_line;
        break;
    case TIOCGPGRP:;
        pid_t *pid = (pid_t *)arg;
        *pid       = session->fgproc;
        break;
    case TCSETS:
    case TCSETSF:
    case TCSETSW: {
        // 对 termios 设置支持，可选实现
        const struct termios *termios = (const struct termios *)arg;
        session->termios.c_lflag      = termios->c_lflag;
        session->termios.c_oflag      = termios->c_oflag;
        session->termios.c_cflag      = termios->c_cflag;
        session->termios.c_line       = termios->c_line;

        session->termios.c_cc[VINTR]    = termios->c_cc[VINTR];  // Ctrl-C
        session->termios.c_cc[VQUIT]    = termios->c_cc[VQUIT];  // Ctrl-\

        session->termios.c_cc[VERASE]   = termios->c_cc[VERASE]; // Backspace
        session->termios.c_cc[VKILL]    = termios->c_cc[VKILL];  // Ctrl-U
        session->termios.c_cc[VEOF]     = termios->c_cc[VEOF];   // Ctrl-D
        session->termios.c_cc[VTIME]    = termios->c_cc[VTIME];
        session->termios.c_cc[VMIN]     = termios->c_cc[VMIN];
        session->termios.c_cc[VSTART]   = termios->c_cc[VSTART];   // Ctrl-Q
        session->termios.c_cc[VSTOP]    = termios->c_cc[VSTOP];    // Ctrl-S
        session->termios.c_cc[VSUSP]    = termios->c_cc[VSUSP];    // Ctrl-Z
        session->termios.c_cc[VREPRINT] = termios->c_cc[VREPRINT]; // Ctrl-R
        session->termios.c_cc[VDISCARD] = termios->c_cc[VDISCARD]; // Ctrl-O
        session->termios.c_cc[VWERASE]  = termios->c_cc[VWERASE];  // Ctrl-W
        session->termios.c_cc[VLNEXT]   = termios->c_cc[VLNEXT];   // Ctrl-V

        break;
    }
    case KDGETMODE:
        *(int *)arg = session->tty_mode;
        break;
    case KDSETMODE:
        session->tty_mode = *(int *)arg;
        break;
    case KDGKBMODE:
        *(int *)arg = session->tty_kbmode;
        break;
    case KDSKBMODE:
        session->tty_kbmode = *(int *)arg;
        break;
    case VT_SETMODE: {
        struct vt_mode *src     = (struct vt_mode *)arg;
        session->vt_mode.mode   = src->mode;
        session->vt_mode.waitv  = src->waitv;
        session->vt_mode.relsig = src->relsig;
        session->vt_mode.acqsig = src->acqsig;
        session->vt_mode.frsig  = src->frsig;
        break;
    }
    case VT_GETMODE: {
        struct vt_mode *src = (struct vt_mode *)arg;
        src->mode           = session->vt_mode.mode;
        src->waitv          = session->vt_mode.waitv;
        src->relsig         = session->vt_mode.relsig;
        src->acqsig         = session->vt_mode.acqsig;
        src->frsig          = session->vt_mode.frsig;
        break;
    }
    case VT_OPENQRY:
        *(int *)arg = 1;
        return 0;
    case TIOCSCTTY:
        break;
    case TIOCSPGRP:
        session->fgproc = get_current_task()->process->pid;
        break;
    default:
        return -ENOTTY;
    }
    return EOK;
}

size_t stdin_read(tty_t *session, char *buffer, size_t offset, size_t number) {

    size_t i = 0;
    for (; i < number; i++) {
        char c = (char)kernel_getch();
        if (c == 0x7f) {
            c = '\b';
        }
        if (c == 0x9) {
            c = '\t';
        }
        if (c == '\b') {
            if (session->termios.c_lflag & ECHO)
                session->ops.write(session, "\b \b", 0, 3);
            if (session->termios.c_lflag & ICANON) {
                if (i > 0) {
                    buffer[i--] = '\0';
                    i--;
                }
                continue;
            }
            buffer[i] = session->termios.c_cc[VERASE];
            continue;
        }
        if (session->termios.c_lflag & ECHO)
            printk("%c", c);
        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            i++;
            if (session->termios.c_lflag & ECHO && c == '\r')
                session->ops.write(session, "\n", 0, 1);
            break;
        }
        buffer[i] = c;
    }

    return i;
}

errno_t tty_poll(tty_t *session, size_t events) {
    ssize_t revents = 0;
    // if (events & EPOLLERR || events & EPOLLPRI) return 0;
    if (events & EPOLLIN && (session->queue->size > 0))
        revents |= EPOLLIN;
    if (events & EPOLLOUT)
        revents |= EPOLLOUT;
    return revents;
}

size_t tty_serial_read(tty_t *session, char *buffer, size_t offset, size_t count) {
    tty_device_t *device = session->device;
    return device->ops.read(device, buffer, count);
}

size_t tty_serial_write(tty_t *session, const char *buffer, size_t offset, size_t count) {
    tty_device_t *device = session->device;
    return device->ops.write(device, buffer, count);
}

void tty_serial_flush(tty_t *session) {
}

errno_t create_session_serial(tty_t *session) {
    if (session->device == NULL)
        return -ENODEV;
    session->terminal  = NULL;
    session->ops.read  = tty_serial_read;
    session->ops.write = tty_serial_write;
    session->ops.flush = tty_serial_flush;
    return EOK;
}

tty_t *alloc_tty_session(tty_device_t *device) {
    if (device == NULL)
        return NULL;
    tty_t *session         = calloc(1, sizeof(tty_t));
    session->device        = device;
    session->queue         = create_atom_queue(1024);
    session->tty_kbmode    = K_XLATE;
    session->tty_mode      = KD_TEXT;
    tty_session_ops_t *ops = &session->ops;
    ops->read              = stdin_read;
    ops->ioctl             = tty_ioctl;
    ops->poll              = tty_poll;
    ops->size_t            = tty_size_t;
    termios_init(&session->termios);

    switch (device->type) {
    case TTY_DEVICE_GRAPHI:
        create_session_terminal(session);
        break;
    case TTY_DEVICE_SERIAL:
        create_session_serial(session);
        break;
    default:
        break;
    }
    return session;
}

void init_tty_session() {
    tty_device_t *device = get_tty_device(boot_get_cmdline_param("console"));
    device = device == NULL ? container_of(tty_device_list.prev, tty_device_t, node) : device;
    not_null_assert(device, "no tty device error.");
    kernel_session  = alloc_tty_session(device);
    current_session = kernel_session;

    input_handler_t *handler = malloc(sizeof(input_handler_t));
    handler->disconnect      = NULL;
    handler->connect         = NULL;
    handler->handle          = tty_event_handle;
    handler->id              = INPUT_KEYBOARD_ID;
    register_input_handler(handler);
}

void init_console_symlink() {
    const char *console = boot_get_cmdline_param("console");
    if (console == NULL) {
        console = "tty0";
    }

    char buf[50];
    sprintf(buf, "/dev/%s", console);

    const vfs_node_t console_node = vfs_open(buf);
    if (console_node == NULL) {
        strcpy(buf, "/dev/tty0");
    } else {
        vfs_close(console_node);
    }

    vfs_symlink("/dev/tty", buf);
    vfs_symlink("/dev/console", "/dev/tty");
    vfs_symlink("/dev/stdout", "/dev/tty");
    vfs_symlink("/dev/stderr", "/dev/tty");
    vfs_symlink("/dev/stdin", "/dev/tty");
}
