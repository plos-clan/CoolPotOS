#include "driver/tty.h"
#include "errno.h"
#include "mem/heap.h"
#include "bootarg.h"
#include "term/terminal.h"

struct llist_header tty_device_list;
tty_t *kernel_session = NULL; // 内核会话

tty_device_t *alloc_tty_device(enum tty_device_type type) {
    tty_device_t *device = (tty_device_t*)calloc(1,sizeof(tty_device_t));
    device->type = type;
    llist_init_head(&device->node);
    return device;
}

errno_t register_tty_device(tty_device_t *device){
    if(device->private_data == NULL) return -EINVAL;
    llist_append(&tty_device_list,&device->node);
    return EOK;
}

tty_device_t *get_tty_device(const char *name){
    if(name == NULL) return NULL;
    tty_device_t *pos = NULL;
    tty_device_t *n = NULL;
    llist_for_each(pos,n,&tty_device_list,node){
        if(strcmp(pos->name,name) == 0){
            return pos;
        }
    }
    return NULL;
}

void init_tty() {
    llist_init_head(&tty_device_list);
    kernel_session = malloc(sizeof(tty_t));
}

void init_tty_session() {
    tty_device_t *device = get_tty_device(boot_get_cmdline_param("console"));
    device = device == NULL ? container_of(tty_device_list.next,tty_device_t ,node) : device;
    kernel_session = calloc(1,sizeof(tty_t));
    kernel_session->device = device;
    create_session_terminal(kernel_session);
}
