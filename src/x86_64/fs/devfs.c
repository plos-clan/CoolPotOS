/**
 * Device Virtual File System
 * 设备虚拟文件系统
 */
#define ALL_IMPLEMENTATION
#include "devfs.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "rbtree-strptr.h"
#include "vdisk.h"
#include "vfs.h"

extern vdisk vdisk_ctl[26]; // vdisk.c

int                devfs_id   = 0;
static vfs_node_t  devfs_root = NULL;
static rbtree_sp_t dev_rbtree;

static int devfs_mount(const char *handle, vfs_node_t node) {
    if (handle != NULL) return VFS_STATUS_FAILED;
    if (devfs_root) {
        kerror("Device file system has been mounted.");
        return VFS_STATUS_FAILED;
    }
    node->fsid = devfs_id;
    devfs_root = node;
    return VFS_STATUS_SUCCESS;
}

static int devfs_mkdir(void *handle, const char *name, vfs_node_t node) {
    node->fsid = 0;
    return 0;
}

static int devfs_stat(void *handle, vfs_node_t node) {
    if (node->type == file_dir) return VFS_STATUS_SUCCESS;
    node->handle = rbtree_sp_get(dev_rbtree, node->name);
    node->type = vdisk_ctl[(uint64_t)node->handle].type == VDISK_STREAM ? file_stream : file_block;
    node->size = disk_size((int)(uint64_t)node->handle);
    return VFS_STATUS_SUCCESS;
}

static void devfs_open(void *parent, const char *name, vfs_node_t node) {
    if (node->type == file_dir) return;
    node->handle = rbtree_sp_get(dev_rbtree, name);
    node->type = vdisk_ctl[(uint64_t)node->handle].type == VDISK_STREAM ? file_stream : file_block;
    node->size = disk_size((int)(uint64_t)node->handle);
}

static int devfs_read(void *file, void *addr, size_t offset, size_t size) {
    int    dev_id = (int)(uint64_t)file;
    size_t sector_size;
    size_t sectors_to_do;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    sector_size                      = vdisk_ctl[dev_id].sector_size;
    size_t padding_up_to_sector_size = PADDING_UP(size, sector_size);
    offset                           = PADDING_UP(offset, sector_size);

    void *buf;
    if (vdisk_ctl[dev_id].type == VDISK_STREAM) goto read;
    if (offset > vdisk_ctl[dev_id].size) return VFS_STATUS_SUCCESS;
    if (vdisk_ctl[dev_id].size < offset + padding_up_to_sector_size) {
        // 计算需要读取的扇区数
        padding_up_to_sector_size = vdisk_ctl[dev_id].size - offset;
        if (size > padding_up_to_sector_size) { size = padding_up_to_sector_size; }
    }
read:

    sectors_to_do = padding_up_to_sector_size / sector_size;
    if (padding_up_to_sector_size == size) {
        buf = malloc(size);
        memset(buf, 0, size);
    } else {
        buf = malloc(padding_up_to_sector_size * 0x1000);
        memset(buf, 0, padding_up_to_sector_size * 0x1000);
    }
    vdisk_read(offset / sector_size, sectors_to_do, buf, dev_id);
    memcpy(addr, buf, size);
    free(buf);
    return VFS_STATUS_SUCCESS;
}

static int devfs_write(void *file, const void *addr, size_t offset, size_t size) {
    int    dev_id = (int)(uint64_t)file;
    size_t sector_size;
    size_t sectors_to_do;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    sector_size                      = vdisk_ctl[dev_id].sector_size;
    size_t padding_up_to_sector_size = PADDING_UP(size, sector_size);
    offset                           = PADDING_UP(offset, sector_size);
    void *buf;
    if (vdisk_ctl[dev_id].type == VDISK_STREAM) goto write;
    if (offset > vdisk_ctl[dev_id].size) return VFS_STATUS_SUCCESS;
    if (vdisk_ctl[dev_id].size < offset + padding_up_to_sector_size) {
        padding_up_to_sector_size = vdisk_ctl[dev_id].size - offset;
        if (size > padding_up_to_sector_size) { size = padding_up_to_sector_size; }
    }
write:
    sectors_to_do = padding_up_to_sector_size / sector_size;

    if (padding_up_to_sector_size == size) {
        buf = malloc(size);
    } else {
        buf = malloc(padding_up_to_sector_size);
        vdisk_read(offset / sector_size, sectors_to_do, buf, dev_id);
    }
    memcpy(buf, addr, size);
    vdisk_write(offset / sector_size, sectors_to_do, buf, dev_id);
    free(buf);
    return VFS_STATUS_SUCCESS;
}

static struct vfs_callback devfs_callbacks = {
    .mount   = devfs_mount,
    .unmount = (void *)empty,
    .mkdir   = devfs_mkdir,
    .close   = (void *)empty,
    .stat    = devfs_stat,
    .open    = devfs_open,
    .read    = devfs_read,
    .write   = devfs_write,
    .mkfile  = (void *)empty,
};

void devfs_setup() {
    devfs_id = vfs_regist("devfs", &devfs_callbacks);
    vfs_mkdir("/dev");
    vfs_node_t dev = vfs_open("/dev");
    if (dev == NULL) {
        kerror("'dev' handle is null.");
        return;
    }
    if (vfs_mount(0, dev) == VFS_STATUS_FAILED) { kerror("Cannot mount device file system."); }
}

void devfs_regist_dev(int drive_id) {
    if (have_vdisk(drive_id)) {
        vfs_child_append(devfs_root, vdisk_ctl[drive_id].drive_name, NULL);
        rbtree_sp_insert(dev_rbtree, vdisk_ctl[drive_id].drive_name, (void *)(uint64_t)drive_id);
    }
}
