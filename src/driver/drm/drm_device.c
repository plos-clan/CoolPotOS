#include "driver/drm/drm_device.h"
#include "krlibc.h"
#include "mem/heap.h"

static uint64_t drm_devices_idxs  = 0;
static cow_arraylist *drm_devices = NULL;

void drm_device_setup() {
    drm_devices = cow_list_create();
}

cow_arraylist *drm_devices_get() {
    return drm_devices;
}

uint64_t drm_device_install(
    int type,
    void *ptr,
    char *name,
    uint64_t parent,
    void *ioctl,
    void *poll,
    void *read,
    void *write,
    void *map
) {
    drmd_device_t *device    = malloc(sizeof(drmd_device_t));
    device->ptr              = ptr;
    device->parent           = parent;
    device->type             = type;
    const uint64_t dev_major = 226;
    const uint64_t dev_minor = drm_devices_idxs++;
    device->dev              = dev_major << 8 | dev_minor;
    device->name             = strdup(name);
    device->ioctl            = ioctl;
    device->poll             = poll;
    device->read             = read;
    device->write            = write;
    device->map              = map;
    device->index            = cow_list_add(drm_devices, device);
    return device->dev;
}
