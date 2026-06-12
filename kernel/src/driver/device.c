#include "driver/device.h"
#include "mem/slub.h"
#include "lock.h"
#include "errno.h"

static struct llist_header device_list;
static uint64_t devices_idxs[DEV_MAX];
static spin_t device_lock = SPIN_INIT;

static device_t *alloc_new_device() {
    return calloc(sizeof(device_t), 1);
}

static bool device_minor_in_use(const int subtype, const uint64_t minor) {
    device_t *ptr;
    device_t *tmp;
    llist_for_each(ptr, tmp, &device_list, node) {
        if (ptr->type == DEV_NULL || ptr->subtype != subtype) {
            continue;
        }
        if ((ptr->dev & 0xFF) == minor) {
            return true;
        }
    }

    return false;
}

static uint64_t device_install0(
    int type,
    int subtype,
    void *ptr,
    char *name,
    uint64_t parent,
    bool use_fixed_minor,
    uint64_t fixed_minor,
    struct device_install_ops ops
) {
    if (subtype < 0 || subtype >= DEV_MAX || !name) {
        return 0;
    }

    if (use_fixed_minor && fixed_minor > 0xFF) {
        return 0;
    }

    spin_lock(device_lock);

    device_t *device = alloc_new_device();
    if (!device) {
        spin_unlock(device_lock);
        return 0;
    }

    uint64_t dev_major = (uint64_t)subtype;
    uint64_t dev_minor = 0;

    if (use_fixed_minor) {
        if (device_minor_in_use(subtype, fixed_minor)) {
            spin_unlock(device_lock);
            free(device);
            return 0;
        }

        dev_minor = fixed_minor;
        if (devices_idxs[subtype] <= dev_minor) {
            devices_idxs[subtype] = dev_minor + 1;
        }
    } else {
        dev_minor = devices_idxs[subtype]++;
    }

    device->ptr     = ptr;
    device->parent  = parent;
    device->type    = type;
    device->subtype = subtype;
    device->dev     = dev_major << 8 | dev_minor;
    device->name    = strdup(name);

    if (!device->name) {
        spin_unlock(device_lock);
        free(device);
        return 0;
    }

    device->open  = ops.open;
    device->close = ops.close;
    device->ioctl = ops.ioctl;
    device->poll  = ops.poll;
    device->read  = ops.read;
    device->write = ops.write;
    device->map   = ops.map;

    uint64_t devnr = device->dev;
    llist_append(&device_list, &device->node);
    spin_unlock(device_lock);

    return devnr;
}

ssize_t device_write(uint64_t dev, void *buf, uint64_t idx, size_t count) {
    device_t *device = device_get(dev);
    if (!device)
        return -ENODEV;
    if (device->write) {
        return device->write(device->ptr, buf, idx, count);
    }
    return -ENOSYS;
}

uint64_t device_install(
    int type,
    int subtype,
    void *ptr,
    char *name,
    uint64_t parent,
    void *open,
    void *close,
    void *ioctl,
    void *poll,
    void *read,
    void *write,
    void *map
) {
    return device_install0(
        type,
        subtype,
        ptr,
        name,
        parent,
        false,
        0,
        (struct device_install_ops){ open, close, ioctl, poll, read, write, map }
    );
}

device_t *device_find(int subtype, uint64_t idx) {
    uint64_t nr = 0;
    device_t *ptr, *tmp;
    llist_for_each(ptr, tmp, &device_list, node) {
        if (ptr->subtype != subtype)
            continue;
        if (nr == idx)
            return ptr;
        nr++;
    }
    return NULL;
}

device_t *device_get(uint64_t dev) {
    device_t *ptr, *tmp;
    llist_for_each(ptr, tmp, &device_list, node) {
        if (ptr->dev == dev)
            return ptr;
    }
    return NULL;
}

void device_init() {
    memset(devices_idxs, 0, sizeof(devices_idxs));
    llist_init_head(&device_list);
}
