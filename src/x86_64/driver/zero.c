#include "errno.h"
#include "krlibc.h"
#include "poll.h"
#include "vdisk.h"

size_t null_read(int id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(addr);
    UNUSED(lba);
    UNUSED(size);
    return 0;
}

size_t null_write(int id, uint8_t *addr, size_t size, size_t lba) {
    UNUSED(id);
    UNUSED(addr);
    UNUSED(lba);
    UNUSED(size);
    return size;
}

size_t zero_read(int id, uint8_t *addr, size_t size, size_t lba) {
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

int zero_ioctl(vdisk *device, size_t req, void *handle) {
    UNUSED(req);
    UNUSED(handle);
    UNUSED(device);
    return -ENOSYS;
}

void zero_setup() {
    vdisk null_device;
    null_device.size       = 0;
    null_device.flag       = 1;
    null_device.type       = VDISK_STREAM;
    null_device.read       = null_read;
    null_device.write      = null_write;
    null_device.read_vbuf  = NULL;
    null_device.write_vbuf = NULL;
    null_device.poll       = zero_poll;
    null_device.ioctl      = zero_ioctl;
    strcpy(null_device.drive_name, "null");
    regist_vdisk(null_device);

    vdisk zero_device;
    zero_device.size       = 0;
    zero_device.flag       = 1;
    zero_device.type       = VDISK_STREAM;
    zero_device.read_vbuf  = NULL;
    zero_device.write_vbuf = NULL;
    zero_device.read       = zero_read;
    zero_device.write      = null_write;
    zero_device.poll       = zero_poll;
    zero_device.ioctl      = zero_ioctl;
    strcpy(zero_device.drive_name, "zero");
    regist_vdisk(zero_device);
}
