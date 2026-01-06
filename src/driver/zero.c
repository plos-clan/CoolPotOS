#include "driver/blk_device.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "task/poll.h"

size_t null_read(void *id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(addr);
    UNUSED(lba);
    UNUSED(size);
    return 0;
}

size_t null_write(void *id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(addr);
    UNUSED(lba);
    UNUSED(size);
    return size;
}

size_t zero_read(void *id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(lba);
    memset(addr, 0, size);
    return size;
}

int zero_poll(size_t events) {
    ssize_t revents = 0;
    if (events & EPOLLIN) revents |= EPOLLIN;
    if (events & EPOLLOUT) revents |= EPOLLOUT;
    return revents;
}

int zero_ioctl(blk_device_t *device, size_t req, void *handle) {
    UNUSED(req);
    UNUSED(handle);
    UNUSED(device);
    return -ENOSYS;
}

void zero_setup() {
    blk_device_t *null_device = malloc(sizeof(blk_device_t));
    null_device->size         = 0;
    null_device->max_size     = 1;
    null_device->block_size   = 1;
    null_device->type         = BLK_STREAM_DEVICE;
    null_device->ops.read     = null_read;
    null_device->ops.write    = null_write;
    null_device->ops.poll     = zero_poll;
    null_device->ops.ioctl    = zero_ioctl;
    null_device->handle       = null_device;
    strcpy(null_device->name, "null");
    register_device(null_device);

    blk_device_t *zero_device = malloc(sizeof(blk_device_t));
    zero_device->size         = 0;
    zero_device->block_size   = 1;
    zero_device->max_size     = 1;
    zero_device->type         = BLK_STREAM_DEVICE;
    zero_device->ops.read     = zero_read;
    zero_device->ops.write    = null_write;
    zero_device->ops.poll     = zero_poll;
    zero_device->ops.ioctl    = zero_ioctl;
    zero_device->handle       = zero_device;
    strcpy(zero_device->name, "zero");
    register_device(zero_device);
}
