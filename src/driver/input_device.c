#include "driver/input_device.h"
#include "mem/heap.h"
#include "cow_arraylist.h"
#include "errno.h"

static cow_arraylist *handlers;
static cow_arraylist *devices;

indev_t *alloc_input_dev() {
    indev_t *device = calloc(1, sizeof(indev_t));
    not_null_assert(device, "indev: alloc new device null.");
    device->handlers = create_llist_queue();
    return device;
}

errno_t register_input_device(indev_t *device) {
    if (device == NULL) {
        return -EINVAL;
    }
    device->index = cow_list_add(devices, device);
    input_handler_t *handler;
    cow_foreach(handlers, handler) {
        if (handler->id == device->id) {
            list_enqueue(device->handlers, handler);
        }
    }
    return EOK;
}

errno_t register_input_handler(input_handler_t *handler) {
    if (handler == NULL) {
        return -EINVAL;
    }
    handler->index = cow_list_add(handlers, handler);
    indev_t *device;
    cow_foreach(devices, device) {
        if (device->id == handler->id) {
            list_enqueue(device->handlers, handler);
        }
    }
    return EOK;
}

static void free_handler(const input_handler_t *handler, indev_t *dev) {
    if (handler->disconnect != NULL) {
        handler->disconnect(dev);
    }
}

errno_t delete_input_device(indev_t *device) {
    if (device == NULL) {
        return -EINVAL;
    }
    cow_list_remove(devices, device->index);
    free_llist_queue(device->handlers, (void *)free_handler, device);
    if (device->name != NULL) {
        free(device->name);
    }
    free(device);
    return EOK;
}

void send_input_event(indev_t *dev, const intype type, const uint64_t code, const uint8_t value) {
    if (dev == NULL) {
        return;
    }
    qlist_foreach(dev->handlers, node) {
        const input_handler_t *handler = node->data;
        handler->handle(dev, type, code, value);
    }
}

void init_input_manager() {
    handlers = cow_list_create();
    devices  = cow_list_create();
}
