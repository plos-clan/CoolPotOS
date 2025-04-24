#include "tty.h"
#include "gop.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "mpmc_queue.h"
#include "terminal.h"

tty_t         defualt_tty;
mpmc_queue_t *queue;
ticketlock    tty_lock;

extern bool open_flush; // terminal.c

static void tty_kernel_print(tty_t *tty, const char *msg) {
    if (open_flush) {
        terminal_puts(msg);
    } else {
        ticket_lock(&tty_lock);
        logkf("");
        terminal_process(msg);
        ticket_unlock(&tty_lock);
    }
}

static void tty_kernel_putc(tty_t *tty, int c) {
    if (open_flush)
        terminal_putc((char)c);
    else {
        ticket_lock(&tty_lock);
        logkf("");
        terminal_process_char((char)c);
        ticket_unlock(&tty_lock);
    }
}

tty_t *alloc_default_tty() {
    tty_t *tty           = malloc(sizeof(tty_t));
    tty->video_ram       = framebuffer->address;
    tty->height          = framebuffer->height;
    tty->width           = framebuffer->width;
    tty->print           = tty_kernel_print;
    tty->putchar         = tty_kernel_putc;
    tty->is_key_wait     = false;
    tty->keyboard_buffer = queue_init();
    return tty;
}

void free_tty(tty_t *tty) {
    if (tty == NULL) return;
    queue_destroy(tty->keyboard_buffer);
    free(tty);
}

tty_t *get_default_tty() {
    return &defualt_tty;
}

void init_tty() {
    defualt_tty.video_ram       = framebuffer->address;
    defualt_tty.width           = framebuffer->width;
    defualt_tty.height          = framebuffer->height;
    defualt_tty.keyboard_buffer = queue_init();
    defualt_tty.print           = tty_kernel_print;
    defualt_tty.putchar         = tty_kernel_putc;
    queue                       = malloc(sizeof(mpmc_queue_t));
    if (init(queue, 1024, sizeof(uint32_t), FAIL) != SUCCESS) {
        kerror("Cannot enable mppmc buffer\n");
        free(queue);
        queue = NULL;
    }
}
