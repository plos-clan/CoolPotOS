#include "driver/urandom.h"
#include "driver/blk_device.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "task/poll.h"

int urandom_poll(size_t events) {
    ssize_t revents = 0;
    if (events & EPOLLIN) revents |= EPOLLIN;
    if (events & EPOLLOUT) revents |= EPOLLOUT;
    return revents;
}

errno_t urandom_ioctl(blk_device_t *device, size_t req, void *handle) {
    return -ENOSYS;
}

size_t urandom_write(void *id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(addr);
    UNUSED(lba);
    UNUSED(size);
    return size;
}

size_t urandom_read(void *id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(lba);
    if (!addr || size == 0) { return 0; }
    if (!arch_get_random_bytes(addr, size)) { return 0; }
    return size;
}

void urandom_init() {
    blk_device_t *urandom_device = malloc(sizeof(blk_device_t));
    urandom_device->size         = 0;
    urandom_device->max_size     = 1;
    urandom_device->block_size   = 1;
    urandom_device->type         = BLK_STREAM_DEVICE;
    urandom_device->ops.ioctl    = urandom_ioctl;
    urandom_device->ops.read     = urandom_read;
    urandom_device->ops.write    = urandom_write;
    urandom_device->ops.poll     = urandom_poll;
    urandom_device->handle       = urandom_device;
    strcpy(urandom_device->name, "urandom");
    register_device(urandom_device);
}
