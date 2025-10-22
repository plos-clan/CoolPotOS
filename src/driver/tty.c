#include "driver/tty.h"
#include "bootarg.h"
#include "driver/input_device.h"
#include "errno.h"
#include "mem/heap.h"
#include "term/terminal.h"
#include "term/klog.h"

struct llist_header tty_device_list;
tty_t              *kernel_session  = NULL; // 内核会话
tty_t              *current_session = NULL; // 当前会话

int kernel_getch() {
    char ch;
    while ((ch = atom_pop(current_session->queue)) == -1) {
        arch_pause();
    }
    return ch;
}

tty_device_t *alloc_tty_device(enum tty_device_type type) {
    tty_device_t *device = (tty_device_t *)calloc(1, sizeof(tty_device_t));
    device->type         = type;
    llist_init_head(&device->node);
    return device;
}

errno_t register_tty_device(tty_device_t *device) {
    if (device->private_data == NULL) return -EINVAL;
    llist_append(&tty_device_list, &device->node);
    return EOK;
}

errno_t delete_tty_device(tty_device_t *device) {
    if (device == NULL) return -EINVAL;
    free(device->private_data);
    llist_delete(&device->node);
    free(device);
    return EOK;
}

tty_device_t *get_tty_device(const char *name) {
    if (name == NULL) return NULL;
    tty_device_t *pos = NULL;
    tty_device_t *n   = NULL;
    llist_for_each(pos, n, &tty_device_list, node) {
        if (strcmp(pos->name, name) == 0) { return pos; }
    }
    return NULL;
}

void init_tty() {
    llist_init_head(&tty_device_list);
    kernel_session = malloc(sizeof(tty_t));
}

void tty_event_handle(indev_t *device, intype type, uint64_t code, uint8_t value) {
    if (type == EV_CHAR) {
        char *ascii_code = (char*)code;
        size_t length = strlen(ascii_code);
        if(length == 0) return;
        for (size_t i = 0; i < length; i++) {
            atom_push(current_session->queue,ascii_code[i]);
        }
    }
}

void init_tty_session() {
    tty_device_t *device = get_tty_device(boot_get_cmdline_param("console"));
    device = device == NULL ? container_of(tty_device_list.prev, tty_device_t, node) : device;
    kernel_session         = calloc(1, sizeof(tty_t));
    kernel_session->device = device;
    kernel_session->queue  = create_atom_queue(1024);
    create_session_terminal(kernel_session);
    current_session = kernel_session;

    input_handler_t *handler = malloc(sizeof(input_handler_t));
    handler->disconnect      = NULL;
    handler->connect         = NULL;
    handler->handle          = tty_event_handle;
    handler->id              = INPUT_KEYBOARD_ID;
    register_input_handler(handler);
}
