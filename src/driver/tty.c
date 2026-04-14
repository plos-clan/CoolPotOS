#include "driver/tty.h"
#include "bootarg.h"
#include "driver/input_device.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "syscall.h"
#include "mem/heap.h"
#include "task/task.h"
#include "term/klog.h"
#include "term/terminal.h"

#if defined(__x86_64__) || defined(__amd64__)
#    include "driver/serial.h"
#endif

static struct llist_header tty_device_list;
static struct llist_header tty_session_list;
static tty_t *kernel_session  = NULL; // 内核会话
static tty_t *current_session = NULL; // 当前会话

static void tty_session_node_name(const tty_t *session, char *buf, size_t buf_len) {
    if (!buf || buf_len == 0) {
        return;
    }

    buf[0] = '\0';
    if (!session || !session->device) {
        return;
    }

    int graphics_idx = 1;
    int serial_idx   = 1;
    tty_t *pos       = NULL;
    tty_t *n         = NULL;
    llist_for_each(pos, n, &tty_session_list, list_node) {
        if (!pos || !pos->device) {
            continue;
        }

        if (pos->device->type == TTY_DEVICE_SERIAL) {
            if (pos == session) {
                snprintf(buf, buf_len, "ttyS%d", serial_idx);
                return;
            }
            serial_idx++;
            continue;
        }

        if (pos == session) {
            snprintf(buf, buf_len, "tty%d", graphics_idx);
            return;
        }
        graphics_idx++;
    }

    strncpy(buf, session->device->name, buf_len - 1);
    buf[buf_len - 1] = '\0';
}

tty_t *get_kernel_session() {
    return kernel_session;
}

tty_t *get_current_session() {
    return current_session;
}

int terminal_getch() {
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

tty_device_t *alloc_tty_device(const enum tty_device_type type) {
    tty_device_t *device = calloc(1, sizeof(tty_device_t));
    device->type         = type;
    llist_init_head(&device->node);
    return device;
}

errno_t register_tty_device(tty_device_t *device) {
    if (device->private_data == NULL) {
        return -EINVAL;
    }
    llist_append(&tty_device_list, &device->node);
    return EOK;
}

errno_t delete_tty_device(tty_device_t *device) {
    if (device == NULL) {
        return -EINVAL;
    }
    free(device->private_data);
    llist_delete(&device->node);
    free(device);
    return EOK;
}

struct llist_header *get_tty_session_list() {
    return &tty_session_list;
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

void init_tty() {
    llist_init_head(&tty_device_list);
    llist_init_head(&tty_session_list);
    kernel_session = malloc(sizeof(tty_t));
}

static void
tty_event_handle(indev_t *device, const intype type, const uint64_t code, uint8_t value) {
    if (type == EV_CHAR) {
        const char *ascii_code = (char *)code;
        const size_t length    = strlen(ascii_code);
        if (length == 0) {
            return;
        }
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

static errno_t tty_ioctl(tty_t *session, const size_t req, void *arg) {
    switch (req) {
    case TIOCGWINSZ:;
        struct winsize *ws = arg;
        if (ws != NULL) {
            size_t col;
            size_t row;
            size_t x;
            size_t y;
            terminal_cols_rows(session, &col, &row);
            terminal_width_height(session, &x, &y);
            ws->ws_col    = col;
            ws->ws_row    = row;
            ws->ws_xpixel = x;
            ws->ws_ypixel = y;
        }
        break;
    case TCGETS:;
        struct termios *termios = arg;
        termios->c_iflag        = session->termios.c_iflag;
        termios->c_oflag        = session->termios.c_oflag;
        termios->c_cflag        = session->termios.c_cflag;
        termios->c_lflag        = session->termios.c_lflag;

        termios->c_cc[VINTR]    = session->termios.c_cc[VINTR]; // Ctrl-C
        termios->c_cc[VQUIT]    = session->termios.c_cc[VQUIT]; // Ctrl-\
        termios->c_cc[VERASE]   = session->termios.c_cc[VERASE]; // Backspace
        termios->c_cc[VKILL]    = session->termios.c_cc[VKILL]; // Ctrl-U
        termios->c_cc[VEOF]     = session->termios.c_cc[VEOF];  // Ctrl-D
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
        if (arg == NULL) {
            return -EINVAL;
        }
        pid_t *pid = arg;
        *pid       = session->fgproc;
        break;
    case TIOCGSID:
        if (arg == NULL) {
            return -EINVAL;
        }
        *(pid_t *)arg = session->sid;
        break;
    case TCSETSF: {
        // 对 termios 设置支持，可选实现
        const struct termios *termios_sw = arg;
        session->termios.c_iflag         = termios_sw->c_iflag;
        session->termios.c_lflag         = termios_sw->c_lflag;
        session->termios.c_oflag         = termios_sw->c_oflag;
        session->termios.c_cflag         = termios_sw->c_cflag;
        session->termios.c_line          = termios_sw->c_line;

        session->termios.c_cc[VINTR]    = termios_sw->c_cc[VINTR]; // Ctrl-C
        session->termios.c_cc[VQUIT]    = termios_sw->c_cc[VQUIT]; // Ctrl-\
        session->termios.c_cc[VERASE]   = termios_sw->c_cc[VERASE]; // Backspace
        session->termios.c_cc[VKILL]    = termios_sw->c_cc[VKILL]; // Ctrl-U
        session->termios.c_cc[VEOF]     = termios_sw->c_cc[VEOF];  // Ctrl-D
        session->termios.c_cc[VTIME]    = termios_sw->c_cc[VTIME];
        session->termios.c_cc[VMIN]     = termios_sw->c_cc[VMIN];
        session->termios.c_cc[VSTART]   = termios_sw->c_cc[VSTART];   // Ctrl-Q
        session->termios.c_cc[VSTOP]    = termios_sw->c_cc[VSTOP];    // Ctrl-S
        session->termios.c_cc[VSUSP]    = termios_sw->c_cc[VSUSP];    // Ctrl-Z
        session->termios.c_cc[VREPRINT] = termios_sw->c_cc[VREPRINT]; // Ctrl-R
        session->termios.c_cc[VDISCARD] = termios_sw->c_cc[VDISCARD]; // Ctrl-O
        session->termios.c_cc[VWERASE]  = termios_sw->c_cc[VWERASE];  // Ctrl-W
        session->termios.c_cc[VLNEXT]   = termios_sw->c_cc[VLNEXT];   // Ctrl-V

        break;
    }
    case KDGETMODE:
        *(int *)arg = session->tty_mode;
        break;
    case KDSETMODE:
        session->tty_mode = (int)arg;
        break;
    case KDGKBMODE:
        *(int *)arg = session->tty_kbmode;
        break;
    case KDSKBMODE:
        session->tty_kbmode = (int)arg;
        break;
    case VT_SETMODE: {
        const struct vt_mode *src = arg;
        session->vt_mode.mode     = src->mode;
        session->vt_mode.waitv    = src->waitv;
        session->vt_mode.relsig   = src->relsig;
        session->vt_mode.acqsig   = src->acqsig;
        session->vt_mode.frsig    = src->frsig;
        break;
    }
    case VT_GETMODE: {
        struct vt_mode *src = arg;
        src->mode           = session->vt_mode.mode;
        src->waitv          = session->vt_mode.waitv;
        src->relsig         = session->vt_mode.relsig;
        src->acqsig         = session->vt_mode.acqsig;
        src->frsig          = session->vt_mode.frsig;
        break;
    }
    case VT_GETSTATE: {
        struct vt_state *state = arg;
        if (state != NULL) {
            state->v_active = 1;
            state->v_state  = 0;
        }
        break;
    }
    case TCSETSW:
    case TCSETS:
        if (!arg || copy_from_user(&session->termios, arg, sizeof(termios))) {
            return -EFAULT;
        }
        return EOK;
    case TCSETS2: {
        struct termios2 t2_set;
        if (!arg || copy_from_user(&t2_set, arg, sizeof(struct termios2))) {
            return -EFAULT;
        }
        memcpy(&session->termios.c_iflag, &t2_set.c_iflag, sizeof(uint32_t));
        memcpy(&session->termios.c_oflag, &t2_set.c_oflag, sizeof(uint32_t));
        memcpy(&session->termios.c_cflag, &t2_set.c_cflag, sizeof(uint32_t));
        memcpy(&session->termios.c_lflag, &t2_set.c_lflag, sizeof(uint32_t));
        session->termios.c_line = t2_set.c_line;
        memcpy(session->termios.c_cc, t2_set.c_cc, sizeof(t2_set.c_cc));
        return EOK;
    }
    case TCGETS2: {
        struct termios2 t2 = { 0 };
        memcpy(&t2.c_iflag, &session->termios.c_iflag, sizeof(uint32_t));
        memcpy(&t2.c_oflag, &session->termios.c_oflag, sizeof(uint32_t));
        memcpy(&t2.c_cflag, &session->termios.c_cflag, sizeof(uint32_t));
        memcpy(&t2.c_lflag, &session->termios.c_lflag, sizeof(uint32_t));
        t2.c_line = session->termios.c_line;
        memcpy(t2.c_cc, session->termios.c_cc, sizeof(t2.c_cc));
        t2.c_ispeed = 0; // Not supported
        t2.c_ospeed = 0; // Not supported
        if (!arg || copy_to_user((void *)arg, &t2, sizeof(struct termios2)))
            return -EFAULT;
        return 0;
    }
    case VT_OPENQRY:
        *(int *)arg = 1;
        return EOK;
    case VT_ACTIVATE:
    case VT_WAITACTIVE:
        return EOK;
    case TCSBRK:
    case TCXONC:
    case TCFLSH:
    case TIOCNXCL:
    case TIOCSWINSZ:
        return EOK;
    case TIOCSCTTY: {
        const tcb_t thread = get_current_task();
        pcb_t proc         = thread == NULL ? NULL : thread->process;
        if (proc == NULL) {
            return -EINVAL;
        }
        proc->tty = session;
        if (proc->ctty_path) {
            free(proc->ctty_path);
        }
        char tty_name[32];
        char path[64];
        tty_session_node_name(session, tty_name, sizeof(tty_name));
        snprintf(path, sizeof(path), "/dev/%s", tty_name[0] ? tty_name : "tty1");
        proc->ctty_path = strdup(path);
        session->fgproc = proc->pgid;
        session->sid    = proc->sid;
        break;
    }
    case TIOCSPGRP:
        if (arg == NULL) {
            return -EINVAL;
        }
        session->fgproc = *(pid_t *)arg;
        break;
    case TIOCNOTTY: {
        return EOK;
    }
    default:
        logkf("no impl tty ioctl: %d\n", req);
        return -EINVAL;
    }
    return EOK;
}

static bool tty_is_erase_char(tty_t *session, const char c) {
    return c == '\b' || c == (char)session->termios.c_cc[VERASE];
}

static void tty_echo_char(tty_t *session, const char c) {
    if (!(session->termios.c_lflag & ECHO)) {
        return;
    }
    session->ops.write(session, &c, 0, 1);
}

static bool tty_serial_has_input(tty_t *session) {
    if (session == NULL || session->device == NULL || session->device->type != TTY_DEVICE_SERIAL) {
        return false;
    }

#if defined(__x86_64__) || defined(__amd64__)
    const struct tty_serial_ *data = session->device->private_data;
    return data != NULL && serial_has_data(data->port);
#else
    return false;
#endif
}

static int tty_serial_getch(tty_t *session) {
    if (session == NULL || session->device == NULL || session->device->ops.read == NULL) {
        return -1;
    }

    char c = 0;
    if (session->device->ops.read(session->device, &c, 1) != 1) {
        return -1;
    }
    return (unsigned char)c;
}

static size_t stdin_read(tty_t *session, char *buffer, size_t offset, const size_t number) {
    const tcb_t tcb = get_current_task() == NULL ? NULL : get_current_task();
    if (tcb != NULL) {
        tcb->status = T_IO_WAIT;
    }
    if (number == 0) {
        if (tcb != NULL) {
            tcb->status = T_RUNNING;
        }
        return 0;
    }

    size_t i             = 0;
    const bool canonical = (session->termios.c_lflag & ICANON) != 0;

    if (!canonical) {
        size_t vmin = session->termios.c_cc[VMIN];
        if (vmin == 0 && session->queue->size == 0) {
            if (tcb != NULL) {
                tcb->status = T_RUNNING;
            }
            return 0;
        }
        if (vmin == 0) {
            vmin = 1;
        }

        while (i < number) {
            char c = (char)terminal_getch();

            if ((session->termios.c_iflag & IGNCR) && c == '\r') {
                continue;
            }
            if ((session->termios.c_iflag & ICRNL) && c == '\r') {
                c = '\n';
            } else if ((session->termios.c_iflag & INLCR) && c == '\n') {
                c = '\r';
            }

            buffer[i] = c;
            tty_echo_char(session, c);

            if (i + 1 >= vmin) {
                i++;
                break;
            }
            i++;
        }

        if (tcb != NULL) {
            tcb->status = T_RUNNING;
        }
        return i;
    }

    while (i < number) {
        char c = (char)terminal_getch();

        if ((session->termios.c_iflag & IGNCR) && c == '\r') {
            continue;
        }
        if ((session->termios.c_iflag & ICRNL) && c == '\r') {
            c = '\n';
        } else if ((session->termios.c_iflag & INLCR) && c == '\n') {
            c = '\r';
        }

        if (tty_is_erase_char(session, c)) {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                if (session->termios.c_lflag & ECHO) {
                    session->ops.write(session, "\b \b", 0, 3);
                }
            }
            continue;
        }

        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            tty_echo_char(session, '\n');
            i++;
            break;
        }

        buffer[i] = c;
        tty_echo_char(session, c);
        i++;
    }
    if (tcb != NULL) {
        tcb->status = T_RUNNING;
    }
    return i;
}

static errno_t tty_poll(tty_t *session, const size_t events) {
    ssize_t revents = 0;
    // if (events & EPOLLERR || events & EPOLLPRI) return 0;
    if (events & EPOLLIN
        && ((session->device->type == TTY_DEVICE_SERIAL && tty_serial_has_input(session))
            || session->queue->size > 0)) {
        revents |= EPOLLIN;
    }
    if (events & EPOLLOUT) {
        revents |= EPOLLOUT;
    }
    return (errno_t)revents;
}

static size_t tty_serial_read(tty_t *session, char *buffer, size_t offset, const size_t count) {
    const tcb_t tcb = get_current_task() == NULL ? NULL : get_current_task();
    if (tcb != NULL) {
        tcb->status = T_IO_WAIT;
    }
    if (count == 0) {
        if (tcb != NULL) {
            tcb->status = T_RUNNING;
        }
        return 0;
    }

    size_t i             = 0;
    const bool canonical = (session->termios.c_lflag & ICANON) != 0;

    if (!canonical) {
        size_t vmin = session->termios.c_cc[VMIN];
        if (vmin == 0 && !tty_serial_has_input(session)) {
            if (tcb != NULL) {
                tcb->status = T_RUNNING;
            }
            return 0;
        }
        if (vmin == 0) {
            vmin = 1;
        }

        while (i < count) {
            int ch = tty_serial_getch(session);
            if (ch < 0) {
                continue;
            }
            char c = (char)ch;

            if ((session->termios.c_iflag & IGNCR) && c == '\r') {
                continue;
            }
            if ((session->termios.c_iflag & ICRNL) && c == '\r') {
                c = '\n';
            } else if ((session->termios.c_iflag & INLCR) && c == '\n') {
                c = '\r';
            }

            buffer[i] = c;
            tty_echo_char(session, c);

            if (i + 1 >= vmin) {
                i++;
                break;
            }
            i++;
        }

        if (tcb != NULL) {
            tcb->status = T_RUNNING;
        }
        return i;
    }

    while (i < count) {
        int ch = tty_serial_getch(session);
        if (ch < 0) {
            continue;
        }
        char c = (char)ch;

        if ((session->termios.c_iflag & IGNCR) && c == '\r') {
            continue;
        }
        if ((session->termios.c_iflag & ICRNL) && c == '\r') {
            c = '\n';
        } else if ((session->termios.c_iflag & INLCR) && c == '\n') {
            c = '\r';
        }

        if (tty_is_erase_char(session, c)) {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                if (session->termios.c_lflag & ECHO) {
                    session->ops.write(session, "\b \b", 0, 3);
                }
            }
            continue;
        }

        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            tty_echo_char(session, '\n');
            i++;
            break;
        }

        buffer[i] = c;
        tty_echo_char(session, c);
        i++;
    }

    if (tcb != NULL) {
        tcb->status = T_RUNNING;
    }
    return i;
}

static size_t
tty_serial_write(tty_t *session, const char *buffer, size_t offset, const size_t count) {
    tty_device_t *device = session->device;
    return device->ops.write(device, buffer, count);
}

static void tty_serial_flush(tty_t *session) {
}

static errno_t create_session_serial(tty_t *session) {
    asserts(session->device, "tty: session device is null.");
    session->terminal  = NULL;
    session->ops.read  = tty_serial_read;
    session->ops.write = tty_serial_write;
    session->ops.flush = tty_serial_flush;
    return EOK;
}

static tty_t *alloc_tty_session(tty_device_t *device) {
    if (device == NULL) {
        return NULL;
    }
    tty_t *session         = calloc(1, sizeof(tty_t));
    session->device        = device;
    session->queue         = create_atom_queue(1024);
    session->tty_kbmode    = K_XLATE;
    session->tty_mode      = KD_TEXT;
    session->lock          = SPIN_INIT;
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
    tty_device_t *pos = NULL;
    tty_device_t *n   = NULL;
    llist_for_each(pos, n, &tty_device_list, node) {
        tty_t *session = alloc_tty_session(pos);
        llist_append(&tty_session_list, &session->list_node);
        if (streq(pos->name, boot_get_cmdline_param("console"))) {
            kernel_session  = session;
            current_session = session;
        }
    }

    if (kernel_session->device->type == TTY_DEVICE_GRAPHI) {
        const struct tty_graphics_ *handle = kernel_session->device->private_data;
        kinfo(
            "TTY(graphics): %dx%d bpp=%d pitch=%d addr=%#p",
            handle->width,
            handle->height,
            handle->bpp,
            handle->pitch,
            handle->address
        );
    }

    input_handler_t *handler = malloc(sizeof(input_handler_t));
    handler->disconnect      = NULL;
    handler->connect         = NULL;
    handler->handle          = tty_event_handle;
    handler->id              = INPUT_KEYBOARD_ID;
    register_input_handler(handler);
}

void init_console_symlink() {
    const char *console = boot_get_cmdline_param("console");
    char buf[50];
    if (console == NULL || streq(console, "tty0")) {
        strcpy(buf, "/dev/tty0");
    } else {
        sprintf(buf, "/dev/%s", console);
    }

    const vfs_node_t console_node = vfs_open(buf);
    if (console_node == NULL) {
        strcpy(buf, "/dev/tty1");
    } else {
        vfs_close(console_node);
    }

    vfs_symlink("/dev/tty", buf);
    vfs_symlink("/dev/console", "/dev/tty");
    vfs_symlink("/dev/stdout", "/dev/tty");
    vfs_symlink("/dev/stderr", "/dev/tty");
    vfs_symlink("/dev/stdin", "/dev/tty");
}
