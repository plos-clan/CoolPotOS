#include "driver/blk_device.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "fs/devtmpfs.h"
#include "fs/partition.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "syscall.h"

static cow_arraylist *block_device_list = NULL;

#define BLK_DEVICE_MAJOR_BASE  240U
#define BLK_DEVICE_MINOR_COUNT 256U

static bool blk_device_is_sysfs_visible(const blk_device_t *device) {
    return device != NULL && device->type != BLK_STREAM_DEVICE;
}

bool blk_device_is_stream(const blk_device_t *device) {
    return device != NULL && device->type == BLK_STREAM_DEVICE;
}

static uint32_t blk_device_major(const blk_device_t *device) {
    return BLK_DEVICE_MAJOR_BASE + (uint32_t)(device->device_id / BLK_DEVICE_MINOR_COUNT);
}

static uint32_t blk_device_minor(const blk_device_t *device) {
    return (uint32_t)(device->device_id % BLK_DEVICE_MINOR_COUNT);
}

uint64_t blk_device_dev_number(const blk_device_t *device) {
    if (!blk_device_is_sysfs_visible(device)) {
        return 0;
    }

    return ((uint64_t)blk_device_major(device) << 8) | blk_device_minor(device);
}

static enum device_type blk_device_node_type(const blk_device_t *device) {
    return blk_device_is_stream(device) ? device_stream : device_block;
}

static char *blk_sysfs_join_path(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *base = vfs_get_fullpath(parent);
    if (base == NULL) {
        return NULL;
    }

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    char *path      = calloc(1, base_len + name_len + 2);
    if (path == NULL) {
        free(base);
        return NULL;
    }

    if (strcmp(base, "/") == 0) {
        snprintf(path, base_len + name_len + 2, "/%s", name);
    } else {
        snprintf(path, base_len + name_len + 2, "%s/%s", base, name);
    }

    free(base);
    return path;
}

static vfs_node_t blk_sysfs_lookup_child(vfs_node_t parent, const char *name) {
    char *path = blk_sysfs_join_path(parent, name);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t node = vfs_open_nofollow(path);
    free(path);
    return node;
}

static void blk_sysfs_ensure_symlink(vfs_node_t parent, const char *name, const char *target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (blk_sysfs_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink(parent, name, target);
}

static void blk_sysfs_ensure_symlink_node(vfs_node_t parent, const char *name, vfs_node_t target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (blk_sysfs_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink_node(parent, name, target);
}

static void blk_sysfs_delete_tree(vfs_node_t node) {
    if (node == NULL) {
        return;
    }

    while (node->child != NULL) {
        vfs_node_t child = node->child->data;
        blk_sysfs_delete_tree(child);
    }

    vfs_delete(node);
    vfs_close(node);
}

static void blk_sysfs_delete_path(const char *path) {
    if (path == NULL) {
        return;
    }

    vfs_node_t node = vfs_open_nofollow(path);
    if (node == NULL) {
        return;
    }

    vfs_delete(node);
    vfs_close(node);
}

static void blk_sysfs_delete_path_tree(const char *path) {
    if (path == NULL) {
        return;
    }

    vfs_node_t node = vfs_open_nofollow(path);
    if (node == NULL) {
        return;
    }

    blk_sysfs_delete_tree(node);
}

static void blk_sysfs_create_u64_file(vfs_node_t parent, const char *name, uint64_t value) {
    char content[64];
    snprintf(content, sizeof(content), "%llu\n", (unsigned long long)value);
    sysfs_create_file(parent, name, content);
}

static size_t blk_sysfs_partition_index(const blk_device_t *device) {
    if (device == NULL || device->type != BLK_PARTITION) {
        return 0;
    }

    const char *suffix = strrchr(device->name, 'p');
    if (suffix == NULL || !isdigit((unsigned char)suffix[1])) {
        return 0;
    }

    return strtoul(suffix + 1, NULL, 10);
}

static void blk_sysfs_format_uevent(const blk_device_t *device, char *buffer, size_t buffer_size) {
    uint32_t major = blk_device_major(device);
    uint32_t minor = blk_device_minor(device);

    if (device->type == BLK_PARTITION) {
        snprintf(
            buffer,
            buffer_size,
            "MAJOR=%u\nMINOR=%u\nDEVNAME=%s\nDEVTYPE=partition\nPARTN=%llu\nSUBSYSTEM=block\n",
            major,
            minor,
            device->name,
            (unsigned long long)blk_sysfs_partition_index(device)
        );
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "MAJOR=%u\nMINOR=%u\nDEVNAME=%s\nDEVTYPE=disk\nSUBSYSTEM=block\n",
        major,
        minor,
        device->name
    );
}

static vfs_node_t blk_sysfs_ensure_virtual_block_root() {
    if (sysfs_get_root() == NULL || sysfs_get_devices_root() == NULL) {
        return NULL;
    }

    vfs_node_t virtual_root = sysfs_ensure_dir(sysfs_get_devices_root(), "virtual");
    if (virtual_root == NULL) {
        return NULL;
    }

    return sysfs_ensure_dir(virtual_root, "block");
}

static vfs_node_t blk_sysfs_get_class_block_root() {
    if (sysfs_get_root() == NULL || sysfs_get_class_root() == NULL) {
        return NULL;
    }

    return sysfs_ensure_dir(sysfs_get_class_root(), "block");
}

static vfs_node_t blk_sysfs_ensure_real_dir(blk_device_t *device) {
    if (!blk_device_is_sysfs_visible(device)) {
        return NULL;
    }

    vfs_node_t block_root = blk_sysfs_ensure_virtual_block_root();
    if (block_root == NULL) {
        return NULL;
    }

    if (device->type == BLK_PARTITION) {
        partition_t *partition = device->handle;
        if (partition == NULL || partition->device == NULL) {
            return NULL;
        }

        vfs_node_t disk_dir = blk_sysfs_ensure_real_dir(partition->device);
        if (disk_dir == NULL) {
            return NULL;
        }

        return sysfs_ensure_dir(disk_dir, device->name);
    }

    return sysfs_ensure_dir(block_root, device->name);
}

static void blk_sysfs_publish_queue(blk_device_t *device, vfs_node_t real_dir) {
    if (device == NULL || real_dir == NULL) {
        return;
    }

    if (device->type == BLK_PARTITION) {
        partition_t *partition = device->handle;
        if (partition == NULL || partition->device == NULL) {
            return;
        }

        vfs_node_t disk_dir = blk_sysfs_ensure_real_dir(partition->device);
        if (disk_dir == NULL) {
            return;
        }

        vfs_node_t queue_dir = sysfs_ensure_dir(disk_dir, "queue");
        if (queue_dir == NULL) {
            return;
        }

        char *queue_path = vfs_get_fullpath(queue_dir);
        if (queue_path == NULL) {
            return;
        }

        blk_sysfs_ensure_symlink(real_dir, "queue", queue_path);
        free(queue_path);
        return;
    }

    vfs_node_t queue_dir = sysfs_ensure_dir(real_dir, "queue");
    if (queue_dir == NULL) {
        return;
    }

    blk_sysfs_create_u64_file(queue_dir, "logical_block_size", device->block_size);
    blk_sysfs_create_u64_file(queue_dir, "physical_block_size", device->block_size);
    blk_sysfs_create_u64_file(queue_dir, "minimum_io_size", device->block_size);
    blk_sysfs_create_u64_file(queue_dir, "optimal_io_size", device->block_size);
    blk_sysfs_create_u64_file(queue_dir, "discard_granularity", 0);
    blk_sysfs_create_u64_file(queue_dir, "discard_max_bytes", 0);
    blk_sysfs_create_u64_file(queue_dir, "discard_zeroes_data", 0);
    blk_sysfs_create_u64_file(queue_dir, "rotational", 0);
}

static void blk_sysfs_publish_device(blk_device_t *device) {
    if (!blk_device_is_sysfs_visible(device) || sysfs_get_root() == NULL) {
        return;
    }

    vfs_node_t real_dir = blk_sysfs_ensure_real_dir(device);
    if (real_dir == NULL) {
        return;
    }

    uint32_t major = blk_device_major(device);
    uint32_t minor = blk_device_minor(device);
    char uevent[160];
    blk_sysfs_format_uevent(device, uevent, sizeof(uevent));

    blk_sysfs_create_u64_file(real_dir, "size", device->size / 512);
    blk_sysfs_create_u64_file(real_dir, "ro", 0);
    blk_sysfs_create_u64_file(real_dir, "removable", 0);
    blk_sysfs_create_u64_file(real_dir, "alignment_offset", 0);
    blk_sysfs_create_u64_file(real_dir, "discard_alignment", 0);
    sysfs_ensure_dir(real_dir, "holders");
    sysfs_ensure_dir(real_dir, "slaves");
    blk_sysfs_publish_queue(device, real_dir);
    blk_sysfs_ensure_symlink(real_dir, "subsystem", "/sys/class/block");
    sysfs_create_file(real_dir, "uevent", uevent);

    char dev_content[32];
    snprintf(dev_content, sizeof(dev_content), "%u:%u\n", major, minor);
    sysfs_create_file(real_dir, "dev", dev_content);

    if (device->type == BLK_PARTITION) {
        partition_t *partition = device->handle;
        if (partition != NULL) {
            blk_sysfs_create_u64_file(real_dir, "partition", blk_sysfs_partition_index(device));
            blk_sysfs_create_u64_file(
                real_dir,
                "start",
                ((uint64_t)partition->starting_lba * partition->sector_size) / 512
            );
        }
    }

    vfs_node_t class_block_root = blk_sysfs_get_class_block_root();
    if (class_block_root != NULL) {
        blk_sysfs_ensure_symlink_node(class_block_root, device->name, real_dir);
    }

    if (device->type == BLK_BLOCK_DEVICE && sysfs_get_block_root() != NULL) {
        blk_sysfs_ensure_symlink_node(sysfs_get_block_root(), device->name, real_dir);
    }

    char *real_path = vfs_get_fullpath(real_dir);
    if (real_path == NULL) {
        return;
    }

    char dev_path[64];
    snprintf(dev_path, sizeof(dev_path), "/sys/dev/block/%u:%u", major, minor);
    if (vfs_open_nofollow(dev_path) == NULL) {
        sysfs_regist_dev('b', major, minor, real_path, device->name, uevent);
    }
    free(real_path);
}

static void blk_sysfs_unpublish_device(blk_device_t *device) {
    if (!blk_device_is_sysfs_visible(device) || sysfs_get_root() == NULL) {
        return;
    }

    uint32_t major = blk_device_major(device);
    uint32_t minor = blk_device_minor(device);
    char path[256];

    snprintf(path, sizeof(path), "/sys/dev/block/%u:%u", major, minor);
    blk_sysfs_delete_path(path);

    snprintf(path, sizeof(path), "/sys/class/block/%s", device->name);
    blk_sysfs_delete_path(path);

    if (device->type == BLK_BLOCK_DEVICE) {
        snprintf(path, sizeof(path), "/sys/block/%s", device->name);
        blk_sysfs_delete_path(path);
        return;
    }

    if (device->type == BLK_PARTITION) {
        partition_t *partition = device->handle;
        if (partition == NULL || partition->device == NULL) {
            return;
        }

        snprintf(
            path,
            sizeof(path),
            "/sys/devices/virtual/block/%s/%s",
            partition->device->name,
            device->name
        );
        blk_sysfs_delete_path_tree(path);
    }
}

static void blk_publish_device_node(blk_device_t *device) {
    if (device == NULL) {
        return;
    }

    vfs_node_t dev_root = vfs_open("/dev");
    if (dev_root == NULL || dev_root->fsid != dev_tmpfs_id) {
        if (dev_root != NULL) {
            vfs_close(dev_root);
        }
        return;
    }

    char path[32] = { 0 };
    sprintf(path, "/dev/%s", device->name);

    vfs_node_t existing = vfs_open(path);
    if (existing != NULL) {
        if (existing->fsid == dev_tmpfs_id && existing->handle != NULL) {
            dtmp_handle_t *handle = existing->handle;
            handle->type          = dtp_file_device;
            handle->dev_type      = blk_device_node_type(device);
            handle->device_handle = device;
            handle->ioctl_t       = (void *)blk_ioctl;
            handle->read_t        = (void *)blk_device_read;
            handle->write_t       = (void *)blk_device_write;
            handle->poll_t        = (void *)blk_poll;
            handle->mapfile_t     = NULL;
            handle->size_t        = (void *)blk_size_t;
            existing->type        = handle->dev_type == device_stream ? file_stream : file_block;
            existing->size        = device->size;
            if (!blk_device_is_stream(device)) {
                existing->dev  = blk_device_dev_number(device);
                existing->rdev = existing->dev;
            }
        }
        vfs_close(existing);
        vfs_close(dev_root);
        return;
    }

    create_device_node(
        dev_root,
        device->name,
        blk_device_node_type(device),
        device,
        blk_device_dev_number(device),
        (void *)blk_ioctl,
        (void *)blk_device_read,
        (void *)blk_device_write,
        (void *)blk_poll,
        NULL,
        (void *)blk_size_t,
        device->dev
    );
    vfs_close(dev_root);
}

static void blk_unpublish_device_node(const char *name) {
    if (name == NULL) {
        return;
    }

    char path[32] = { 0 };
    sprintf(path, "/dev/%s", name);

    vfs_node_t node = vfs_open(path);
    if (node == NULL) {
        return;
    }

    vfs_delete(node);
    vfs_close(node);
}

cow_arraylist *get_block_device_list() {
    return block_device_list;
}

static inline void *blk_alloc_frames_bytes(size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages == 0) {
        pages = 1;
    }

    uint64_t phys = alloc_frames(pages);
    if (phys == 0) {
        return NULL;
    }

    void *virt = phys_to_virt(phys);
    memset(virt, 0, pages * PAGE_SIZE);
    return virt;
}

static inline void blk_free_frames_bytes(void *ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }

    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages == 0) {
        pages = 1;
    }

    free_frames(virt_to_phys(ptr), pages);
}

#define BLK_DMA_ALIGN       PAGE_SIZE
#define BLK_IS_DMA_BUF(ptr) ((((uintptr_t)(ptr)) & (BLK_DMA_ALIGN - 1)) == 0)

static bool blk_buffer_is_userspace(const void *buf, size_t len) {
    return buf != NULL && !check_user_overflow((uint64_t)buf, len);
}

static bool blk_copy_to_buffer(void *dst, const void *src, size_t len, bool dst_is_userspace) {
    if (len == 0) {
        return true;
    }

    if (dst_is_userspace) {
        return copy_to_user(dst, src, len);
    }

    memcpy(dst, src, len);
    return true;
}

static bool blk_copy_from_buffer(void *dst, const void *src, size_t len, bool src_is_userspace) {
    if (len == 0) {
        return true;
    }

    if (src_is_userspace) {
        return copy_from_user(dst, src, len);
    }

    memcpy(dst, src, len);
    return true;
}

size_t blk_device_read(
    const blk_device_t *device, void *buffer, const size_t offset, const size_t length
) {
    if (device == NULL) {
        return -1;
    }
    if (device->ops.read == NULL) {
        return -1;
    }

    if (device->type == BLK_STREAM_DEVICE) {
        return device->ops.read(device->handle, buffer, offset, length);
    }

    const uint64_t block_size = device->block_size;
    const uint64_t max_sec    = device->max_size >= block_size ? device->max_size / block_size : 1;
    uint8_t *dst              = buffer;
    uint64_t sector           = offset / block_size;
    uint64_t block_off        = offset % block_size;
    uint64_t remaining        = length;
    uint64_t total            = 0;
    bool dst_is_userspace     = blk_buffer_is_userspace(buffer, length);

    if (!dst_is_userspace && block_off == 0 && (length % block_size) == 0 && BLK_IS_DMA_BUF(dst)) {
        uint64_t secs_left = length / block_size;
        while (secs_left > 0) {
            uint64_t n = MIN(secs_left, max_sec);
            if (device->ops.read(device->handle, dst, n, sector) != n) {
                return (uint64_t)-1;
            }

            uint64_t bytes = n * block_size;
            dst += bytes;
            sector += n;
            secs_left -= n;
            total += bytes;
        }
        return total;
    }

    if (block_off != 0) {
        uint64_t head   = MIN(block_size - block_off, remaining);
        uint8_t *bounce = blk_alloc_frames_bytes(block_size);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        if (device->ops.read(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (!blk_copy_to_buffer(dst, bounce + block_off, head, dst_is_userspace)) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }

        blk_free_frames_bytes(bounce, block_size);
        dst += head;
        remaining -= head;
        total += head;
        sector++;
    }

    uint64_t mid_secs = remaining / block_size;
    if (mid_secs > 0 && !dst_is_userspace && BLK_IS_DMA_BUF(dst)) {
        while (mid_secs > 0) {
            uint64_t n = MIN(mid_secs, max_sec);
            if (device->ops.read(device->handle, dst, n, sector) != n) {
                return (uint64_t)-1;
            }

            uint64_t bytes = n * block_size;
            dst += bytes;
            remaining -= bytes;
            total += bytes;
            sector += n;
            mid_secs -= n;
        }
    } else if (mid_secs > 0) {
        uint64_t bn     = MIN(mid_secs, max_sec);
        uint64_t bsz    = bn * block_size;
        uint8_t *bounce = blk_alloc_frames_bytes(bsz);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        while (mid_secs > 0) {
            uint64_t n = MIN(mid_secs, bn);
            if (device->ops.read(device->handle, bounce, n, sector) != n) {
                blk_free_frames_bytes(bounce, bsz);
                return (uint64_t)-1;
            }

            uint64_t bytes = n * block_size;
            if (!blk_copy_to_buffer(dst, bounce, bytes, dst_is_userspace)) {
                blk_free_frames_bytes(bounce, bsz);
                return (uint64_t)-1;
            }

            dst += bytes;
            remaining -= bytes;
            total += bytes;
            sector += n;
            mid_secs -= n;
        }

        blk_free_frames_bytes(bounce, bsz);
    }

    if (remaining > 0) {
        uint8_t *bounce = blk_alloc_frames_bytes(block_size);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        if (device->ops.read(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (!blk_copy_to_buffer(dst, bounce, remaining, dst_is_userspace)) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }

        blk_free_frames_bytes(bounce, block_size);
        total += remaining;
    }

    return total;
}

size_t blk_device_write(
    const blk_device_t *device, const void *buffer, const size_t offset, const size_t length
) {
    if (device == NULL) {
        return -1;
    }
    if (device->ops.write == NULL) {
        return -1;
    }

    if (device->type == BLK_STREAM_DEVICE) {
        return device->ops.write(device->handle, (uint8_t *)buffer, offset, length);
    }

    const uint64_t block_size = device->block_size;
    const uint64_t max_sec    = device->max_size >= block_size ? device->max_size / block_size : 1;
    const uint8_t *src        = buffer;
    uint64_t sector           = offset / block_size;
    uint64_t block_off        = offset % block_size;
    uint64_t remaining        = length;
    uint64_t total            = 0;
    bool src_is_userspace     = blk_buffer_is_userspace(buffer, length);

    if (!src_is_userspace && block_off == 0 && (length % block_size) == 0 && BLK_IS_DMA_BUF(src)) {
        uint64_t secs_left = length / block_size;
        while (secs_left > 0) {
            uint64_t n = MIN(secs_left, max_sec);
            if (device->ops.write(device->handle, (uint8_t *)src, n, sector) != n) {
                return (uint64_t)-1;
            }

            uint64_t bytes = n * block_size;
            src += bytes;
            sector += n;
            secs_left -= n;
            total += bytes;
        }
        return total;
    }

    if (block_off != 0) {
        if (device->ops.read == NULL) {
            return (uint64_t)-1;
        }

        uint64_t head   = MIN(block_size - block_off, remaining);
        uint8_t *bounce = blk_alloc_frames_bytes(block_size);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        if (device->ops.read(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (!blk_copy_from_buffer(bounce + block_off, src, head, src_is_userspace)) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (device->ops.write(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }

        blk_free_frames_bytes(bounce, block_size);
        src += head;
        remaining -= head;
        total += head;
        sector++;
    }

    uint64_t mid_secs = remaining / block_size;
    if (mid_secs > 0 && !src_is_userspace && BLK_IS_DMA_BUF(src)) {
        while (mid_secs > 0) {
            uint64_t n = MIN(mid_secs, max_sec);
            if (device->ops.write(device->handle, (uint8_t *)src, n, sector) != n) {
                return (uint64_t)-1;
            }

            uint64_t bytes = n * block_size;
            src += bytes;
            remaining -= bytes;
            total += bytes;
            sector += n;
            mid_secs -= n;
        }
    } else if (mid_secs > 0) {
        uint64_t bn     = MIN(mid_secs, max_sec);
        uint64_t bsz    = bn * block_size;
        uint8_t *bounce = blk_alloc_frames_bytes(bsz);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        while (mid_secs > 0) {
            uint64_t n     = MIN(mid_secs, bn);
            uint64_t bytes = n * block_size;
            if (!blk_copy_from_buffer(bounce, src, bytes, src_is_userspace)) {
                blk_free_frames_bytes(bounce, bsz);
                return (uint64_t)-1;
            }
            if (device->ops.write(device->handle, bounce, n, sector) != n) {
                blk_free_frames_bytes(bounce, bsz);
                return (uint64_t)-1;
            }

            src += bytes;
            remaining -= bytes;
            total += bytes;
            sector += n;
            mid_secs -= n;
        }

        blk_free_frames_bytes(bounce, bsz);
    }

    if (remaining > 0) {
        if (device->ops.read == NULL) {
            return (uint64_t)-1;
        }

        uint8_t *bounce = blk_alloc_frames_bytes(block_size);
        if (bounce == NULL) {
            return (uint64_t)-1;
        }

        if (device->ops.read(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (!blk_copy_from_buffer(bounce, src, remaining, src_is_userspace)) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }
        if (device->ops.write(device->handle, bounce, 1, sector) != 1) {
            blk_free_frames_bytes(bounce, block_size);
            return (uint64_t)-1;
        }

        blk_free_frames_bytes(bounce, block_size);
        total += remaining;
    }

    return total;
}

size_t blk_size_t(const blk_device_t *device) {
    return device->size;
}

errno_t blk_ioctl(blk_device_t *device, const size_t cmd, void *arg) {
    switch (cmd) {
    case BLKGETSIZE64:
        *(uint64_t *)arg = device->size;
        break;
    case BLKGETSIZE:
        *(unsigned long *)arg = device->size / device->block_size;
        break;
    case BLKSSZGET:
        *(int *)arg = (int)device->block_size;
        break;
    case BLKRRPART:
        if (device->type != BLK_BLOCK_DEVICE) {
            return -ENOSYS;
        }
        parser_block_device(device);
        break;
    default:
        return -ENOSYS;
    }
    return EOK;
}

errno_t blk_poll(blk_device_t *device, const size_t events) {
    return (errno_t)events;
}

errno_t delete_blk_device(const size_t blk_id) {
    blk_device_t *device = cow_list_clear(block_device_list, blk_id);
    if (device == NULL) {
        return -ENODEV;
    }

    blk_unpublish_device_node(device->name);
    blk_sysfs_unpublish_device(device);

    errno_t res = EOK;
    if (device->ops.del_blk != NULL) {
        res = device->ops.del_blk(device->handle);
    }
    free(device);
    return res;
}

size_t register_device(blk_device_t *device) {
    if (device == NULL || device->handle == NULL) {
        return -ENODEV;
    }
    device->device_id = cow_list_add(block_device_list, device);
    blk_publish_device_node(device);
    blk_sysfs_publish_device(device);
    if (device->type == BLK_BLOCK_DEVICE) {
        parser_block_device(device);
    }
    return device->device_id;
}

void blk_sysfs_populate() {
    if (sysfs_get_root() == NULL) {
        return;
    }

    blk_device_t *device = NULL;
    cow_foreach(block_device_list, device) {
        blk_sysfs_publish_device(device);
    }
}

void init_block_device_manager() {
    block_device_list = cow_list_create();
}
