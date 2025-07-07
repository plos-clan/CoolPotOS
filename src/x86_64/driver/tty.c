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

tty_t        *defualt_tty = NULL;
mpmc_queue_t *queue;
spin_t        tty_lock          = SPIN_INIT;
atom_queue   *temp_stdin_buffer = NULL;
bool          ioSwitch          = false;

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
    tty_t *tty     = malloc(sizeof(tty_t));
    tty->video_ram = framebuffer->address;
    tty->height    = framebuffer->height;
    tty->width     = framebuffer->width;
    tty->print     = tty_kernel_print;
    tty->putchar   = tty_kernel_putc;
    tty->mode      = ECHO;
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
        if (get_current_task()->parent_group->tty->mode == ECHO) {
            if (c == '\b') {
                printk("\b \b");
                if (i > 0) {
                    buffer[i--] = '\0';
                    i--;
                }
                continue;
            }
            printk("%c", c);
        }
        if (c == '\n' || c == '\r') {
            buffer[i] = 0x0a;
            i++;
            if (get_current_task()->parent_group->tty->mode == ECHO && c == '\r') printk("\n");
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
        struct termios *term = (struct termios *)arg;

        term->c_iflag = BRKINT | ICRNL | IXON;
        term->c_oflag = OPOST;
        term->c_cflag = CREAD | CS8;
        term->c_lflag = get_current_task()->parent_group->tty->mode;

        term->c_cc[VINTR]  = 3;   // Ctrl-C
        term->c_cc[VQUIT]  = 28;  // Ctrl-\

        term->c_cc[VERASE] = 127; // Backspace
        term->c_cc[VKILL]  = 21;  // Ctrl-U
        term->c_cc[VEOF]   = 4;   // Ctrl-D
        term->c_cc[VTIME]  = 0;
        term->c_cc[VMIN]   = 1;

        term->c_line = 0;

        term->c_ispeed = 96000;
        term->c_ospeed = 96000;
        break;
    case TIOCGPGRP:
        int *pid = (int *)arg;
        *pid     = get_current_task()->pid;
        break;
    default: return -ENOTTY;
    }
    return EOK;
}

static int tty_poll(size_t events) {
    ssize_t revents = 0;
    if (events & EPOLLERR || events & EPOLLPRI) return 0;

    if (events & EPOLLIN) revents |= EPOLLIN;
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
    defualt_tty->mode      = ECHO;
    defualt_tty->print     = tty_kernel_print;
    defualt_tty->putchar   = tty_kernel_putc;
    queue                  = malloc(sizeof(mpmc_queue_t));
    if (init(queue, 1024, sizeof(uint32_t), FAIL) != SUCCESS) {
        kerror("Cannot enable mpmc buffer\n");
        free(queue);
        queue = NULL;
    }
    temp_stdin_buffer = create_atom_queue(2048);
    build_tty_device();
    kinfo("Default tty device init done.");
}
