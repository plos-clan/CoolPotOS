#include "driver/tty.h"

#include "boot.h"
#include "mem/slub.h"
#include "errno.h"

static struct llist_header tty_device_list;
static tty_t *kernel_session = NULL; // 内核会话

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

void tty_init() {
    llist_init_head(&tty_device_list);
    kernel_session = malloc(sizeof(tty_t));
    termios_init(&kernel_session->term);
}
