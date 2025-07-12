#include "tty.h"
#include "atom_queue.h"
#include "gop.h"
#include "ioctl.h"
#include "keyboard.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "mpmc_queue.h"
#include "pcb.h"
#include "terminal.h"
#include "vdisk.h"

tty_t         *defualt_tty = NULL;
mpmc_queue_t  *queue;
spin_t         tty_lock          = SPIN_INIT;
atom_queue    *temp_stdin_buffer = NULL;
bool           ioSwitch          = false;
int            tty_mode          = KD_TEXT;
int            tty_kbmode        = K_XLATE;
struct vt_mode current_vt_mode   = {0};

extern bool open_flush; // terminal.c

static void tty_kernel_print(tty_t *tty, const char *msg) {
    if (open_flush) {
        terminal_puts(msg);
    } else {
        spin_lock(tty_lock);
        for (size_t i = 0; msg[i] != '\0'; i++) {
            char c = msg[i];
            if (c == '\n') {
                terminal_process_byte('\r');
                update_terminal();
            }
            terminal_process_byte(c);
        }
        spin_unlock(tty_lock);
    }
}

static void tty_kernel_putc(tty_t *tty, int c) {
    if (open_flush) {
        if (c == '\n') { terminal_putc('\r'); }
        terminal_putc((char)c);
    } else {
        spin_lock(tty_lock);
        if (c == '\n') {
            terminal_process_byte('\r');
            update_terminal();
        }
        terminal_process_byte(c);
        spin_unlock(tty_lock);
    }
}

tty_t *alloc_default_tty() {
    tty_t *tty           = malloc(sizeof(tty_t));
    tty->video_ram       = framebuffer->address;
    tty->height          = framebuffer->height;
    tty->width           = framebuffer->width;
    tty->print           = tty_kernel_print;
    tty->putchar         = tty_kernel_putc;
    tty->termios.c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    tty->termios.c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
    tty->termios.c_oflag = OPOST;
    tty->termios.c_cflag = CS8 | CREAD | CLOCAL;

    // Initialize other control characters to 0
    for (int i = 16; i < NCCS; i++) {
        tty->termios.c_cc[i] = 0;
    }

    tty->termios.c_line         = 0;
    tty->termios.c_cc[VINTR]    = 3;    // Ctrl-C
    tty->termios.c_cc[VQUIT]    = 28;   // Ctrl-Q
    tty->termios.c_cc[VERASE]   = 0x7F; // DEL
    tty->termios.c_cc[VKILL]    = 21;   // Ctrl-U
    tty->termios.c_cc[VEOF]     = 4;    // Ctrl-D
    tty->termios.c_cc[VTIME]    = 0;    // No timer
    tty->termios.c_cc[VMIN]     = 1;    // Return each byte
    tty->termios.c_cc[VSTART]   = 17;   // Ctrl-Q
    tty->termios.c_cc[VSTOP]    = 19;   // Ctrl-S
    tty->termios.c_cc[VSUSP]    = 26;   // Ctrl-Z
    tty->termios.c_cc[VREPRINT] = 18;   // Ctrl-R
    tty->termios.c_cc[VDISCARD] = 15;   // Ctrl-O
    tty->termios.c_cc[VWERASE]  = 23;   // Ctrl-W
    tty->termios.c_cc[VLNEXT]   = 22;   // Ctrl-V

    return tty;
}

void free_tty(tty_t *tty) {
    if (tty == NULL) return;
    free(tty);
}

tty_t *get_default_tty() {
    return defualt_tty;
}

static int tty_getch() {
    if (temp_stdin_buffer->size > 0) { return atom_pop(temp_stdin_buffer); }
    return kernel_getch();
}

static size_t stdin_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    bool is_sti = are_interrupts_enabled();
    open_interrupt;

    size_t i = 0;
    for (; i < number; i++) {
        char c = (char)tty_getch();
        if (c == 0x7f) { c = '\b'; }
        if (c == 0x9) { c = '\t'; }
        if (c == '\b') {
            if (get_current_task()->parent_group->tty->termios.c_lflag & ECHO) printk("\b \b");
            if (get_current_task()->parent_group->tty->termios.c_lflag & ICANON) {
                if (i > 0) {
                    buffer[i--] = '\0';
                    i--;
                }
                continue;
            } else {
                buffer[i] = get_current_task()->parent_group->tty->termios.c_cc[VERASE];
                continue;
            }
        }
        if (get_current_task()->parent_group->tty->termios.c_lflag & ECHO) printk("%c", c);
        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            i++;
            if (get_current_task()->parent_group->tty->termios.c_lflag & ECHO && c == '\r')
                printk("\n");
            break;
        }
        buffer[i] = c;
    }

    if (!is_sti) open_interrupt;

    return i;
}

static size_t stdout_write(int drive, uint8_t *buffer, size_t number, size_t lba) {
    tty_t *tty;
    if (get_current_task() == NULL) {
        tty = get_default_tty();
    } else
        tty = get_current_task()->parent_group->tty;

    for (size_t i = 0; i < number; i++) {
        tty->putchar(tty, buffer[i]);
    }

    return number;
}

static int tty_ioctl(size_t req, void *arg) {
    switch (req) {
    case TIOCGWINSZ:
        struct winsize *ws = (struct winsize *)arg;
        if (ws != NULL) {
            ws->ws_row    = get_terminal_row(get_current_task()->parent_group->tty->height);
            ws->ws_col    = get_terminal_col(get_current_task()->parent_group->tty->width);
            ws->ws_xpixel = get_current_task()->parent_group->tty->width;
            ws->ws_ypixel = get_current_task()->parent_group->tty->height;
        }
        break;
    case TCGETS:
        struct termios *termios = (struct termios *)arg;

        termios->c_iflag = get_current_task()->parent_group->tty->termios.c_iflag;
        termios->c_oflag = get_current_task()->parent_group->tty->termios.c_oflag;
        termios->c_cflag = get_current_task()->parent_group->tty->termios.c_cflag;
        termios->c_lflag = get_current_task()->parent_group->tty->termios.c_lflag;

        termios->c_cc[VINTR] = get_current_task()->parent_group->tty->termios.c_cc[VINTR]; // Ctrl-C
        termios->c_cc[VQUIT] = get_current_task()->parent_group->tty->termios.c_cc[VQUIT]; // Ctrl-\

        termios->c_cc[VERASE] =
            get_current_task()->parent_group->tty->termios.c_cc[VERASE]; // Backspace
        termios->c_cc[VKILL] = get_current_task()->parent_group->tty->termios.c_cc[VKILL]; // Ctrl-U
        termios->c_cc[VEOF]  = get_current_task()->parent_group->tty->termios.c_cc[VEOF];  // Ctrl-D
        termios->c_cc[VTIME] = get_current_task()->parent_group->tty->termios.c_cc[VTIME];
        termios->c_cc[VMIN]  = get_current_task()->parent_group->tty->termios.c_cc[VMIN];
        termios->c_cc[VSTART] =
            get_current_task()->parent_group->tty->termios.c_cc[VSTART];                   // Ctrl-Q
        termios->c_cc[VSTOP] = get_current_task()->parent_group->tty->termios.c_cc[VSTOP]; // Ctrl-S
        termios->c_cc[VSUSP] = get_current_task()->parent_group->tty->termios.c_cc[VSUSP]; // Ctrl-Z
        termios->c_cc[VREPRINT] =
            get_current_task()->parent_group->tty->termios.c_cc[VREPRINT]; // Ctrl-R
        termios->c_cc[VDISCARD] =
            get_current_task()->parent_group->tty->termios.c_cc[VDISCARD]; // Ctrl-O
        termios->c_cc[VWERASE] =
            get_current_task()->parent_group->tty->termios.c_cc[VWERASE]; // Ctrl-W
        termios->c_cc[VLNEXT] =
            get_current_task()->parent_group->tty->termios.c_cc[VLNEXT]; // Ctrl-V

        termios->c_line = get_current_task()->parent_group->tty->termios.c_line;
        break;
    case TIOCGPGRP:
        int *pid = (int *)arg;
        *pid     = get_current_task()->pid;
        break;
    case TCSETS:
    case TCSETSF:
    case TCSETSW: {
        // 对 termios 设置支持，可选实现
        const struct termios *termios                          = (const struct termios *)arg;
        get_current_task()->parent_group->tty->termios.c_lflag = termios->c_lflag;
        get_current_task()->parent_group->tty->termios.c_oflag = termios->c_oflag;
        get_current_task()->parent_group->tty->termios.c_cflag = termios->c_cflag;
        get_current_task()->parent_group->tty->termios.c_line  = termios->c_line;

        get_current_task()->parent_group->tty->termios.c_cc[VINTR] = termios->c_cc[VINTR]; // Ctrl-C
        get_current_task()->parent_group->tty->termios.c_cc[VQUIT] = termios->c_cc[VQUIT]; // Ctrl-\

        get_current_task()->parent_group->tty->termios.c_cc[VERASE] =
            termios->c_cc[VERASE]; // Backspace
        get_current_task()->parent_group->tty->termios.c_cc[VKILL] = termios->c_cc[VKILL]; // Ctrl-U
        get_current_task()->parent_group->tty->termios.c_cc[VEOF]  = termios->c_cc[VEOF];  // Ctrl-D
        get_current_task()->parent_group->tty->termios.c_cc[VTIME] = termios->c_cc[VTIME];
        get_current_task()->parent_group->tty->termios.c_cc[VMIN]  = termios->c_cc[VMIN];
        get_current_task()->parent_group->tty->termios.c_cc[VSTART] =
            termios->c_cc[VSTART];                                                         // Ctrl-Q
        get_current_task()->parent_group->tty->termios.c_cc[VSTOP] = termios->c_cc[VSTOP]; // Ctrl-S
        get_current_task()->parent_group->tty->termios.c_cc[VSUSP] = termios->c_cc[VSUSP]; // Ctrl-Z
        get_current_task()->parent_group->tty->termios.c_cc[VREPRINT] =
            termios->c_cc[VREPRINT]; // Ctrl-R
        get_current_task()->parent_group->tty->termios.c_cc[VDISCARD] =
            termios->c_cc[VDISCARD]; // Ctrl-O
        get_current_task()->parent_group->tty->termios.c_cc[VWERASE] =
            termios->c_cc[VWERASE]; // Ctrl-W
        get_current_task()->parent_group->tty->termios.c_cc[VLNEXT] =
            termios->c_cc[VLNEXT]; // Ctrl-V

        break;
    }
    case KDGETMODE: *(int *)arg = tty_mode; break;
    case KDSETMODE: tty_mode = *(int *)arg; break;
    case KDGKBMODE: *(int *)arg = tty_kbmode; break;
    case KDSKBMODE: tty_kbmode = *(int *)arg; break;
    case VT_SETMODE: {
        struct vt_mode *src    = (struct vt_mode *)arg;
        current_vt_mode.mode   = src->mode;
        current_vt_mode.waitv  = src->waitv;
        current_vt_mode.relsig = src->relsig;
        current_vt_mode.acqsig = src->acqsig;
        current_vt_mode.frsig  = src->frsig;
        break;
    }
    case VT_GETMODE: {
        struct vt_mode *src = (struct vt_mode *)arg;
        src->mode           = current_vt_mode.mode;
        src->waitv          = current_vt_mode.waitv;
        src->relsig         = current_vt_mode.relsig;
        src->acqsig         = current_vt_mode.acqsig;
        src->frsig          = current_vt_mode.frsig;
        break;
    }
    case VT_OPENQRY: *(int *)arg = 1; return 0;
    default: return -ENOTTY;
    }
    return EOK;
}

static int tty_poll(size_t events) {
    ssize_t            revents = 0;
    // if (events & EPOLLERR || events & EPOLLPRI) return 0;
    extern atom_queue *temp_keyboard_buffer;
    if (events & EPOLLIN && (temp_keyboard_buffer->size > 0)) revents |= EPOLLIN;
    if (events & EPOLLOUT) revents |= EPOLLOUT;
    ioSwitch = !ioSwitch;
    return revents;
}

void build_tty_device() {
    vdisk stdio;
    stdio.type = VDISK_STREAM;
    strcpy(stdio.drive_name, "stdio");
    stdio.flag        = 1;
    stdio.sector_size = 1;
    stdio.size        = 1;
    stdio.read        = stdin_read;
    stdio.write       = stdout_write;
    stdio.ioctl       = tty_ioctl;
    stdio.poll        = tty_poll;
    stdio.map         = (void *)empty;
    regist_vdisk(stdio);

    vdisk ttydev;
    ttydev.type = VDISK_STREAM;
    strcpy(ttydev.drive_name, "tty0");
    ttydev.flag        = 1;
    ttydev.sector_size = 1;
    ttydev.size        = 1;
    ttydev.read        = stdin_read;
    ttydev.write       = stdout_write;
    ttydev.ioctl       = tty_ioctl;
    ttydev.poll        = tty_poll;
    ttydev.map         = (void *)empty;
    regist_vdisk(ttydev);
}

void init_tty() {
    defualt_tty            = malloc(sizeof(tty_t));
    defualt_tty->video_ram = framebuffer->address;
    defualt_tty->width     = framebuffer->width;
    defualt_tty->height    = framebuffer->height;

    defualt_tty->termios.c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    defualt_tty->termios.c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
    defualt_tty->termios.c_oflag = OPOST;
    defualt_tty->termios.c_cflag = CS8 | CREAD | CLOCAL;

    // Initialize other control characters to 0
    for (int i = 16; i < NCCS; i++) {
        defualt_tty->termios.c_cc[i] = 0;
    }

    defualt_tty->termios.c_line         = 0;
    defualt_tty->termios.c_cc[VINTR]    = 3;  // Ctrl-C
    defualt_tty->termios.c_cc[VQUIT]    = 28; // Ctrl-tty->termios.c_cc[VERASE] = 127; // DEL
    defualt_tty->termios.c_cc[VKILL]    = 21; // Ctrl-U
    defualt_tty->termios.c_cc[VEOF]     = 4;  // Ctrl-D
    defualt_tty->termios.c_cc[VTIME]    = 0;  // No timer
    defualt_tty->termios.c_cc[VMIN]     = 1;  // Return each byte
    defualt_tty->termios.c_cc[VSTART]   = 17; // Ctrl-Q
    defualt_tty->termios.c_cc[VSTOP]    = 19; // Ctrl-S
    defualt_tty->termios.c_cc[VSUSP]    = 26; // Ctrl-Z
    defualt_tty->termios.c_cc[VREPRINT] = 18; // Ctrl-R
    defualt_tty->termios.c_cc[VDISCARD] = 15; // Ctrl-O
    defualt_tty->termios.c_cc[VWERASE]  = 23; // Ctrl-W
    defualt_tty->termios.c_cc[VLNEXT]   = 22; // Ctrl-V

    defualt_tty->print   = tty_kernel_print;
    defualt_tty->putchar = tty_kernel_putc;
    queue                = malloc(sizeof(mpmc_queue_t));
    if (init(queue, 1024, sizeof(uint32_t), FAIL) != SUCCESS) {
        kerror("Cannot enable mpmc buffer\n");
        free(queue);
        queue = NULL;
    }
    temp_stdin_buffer = create_atom_queue(2048);
    build_tty_device();
    kinfo("Default tty device init done.");
}
