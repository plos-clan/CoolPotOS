/**
 * Device Virtual File System
 * 设备虚拟文件系统
 */
#define ALL_IMPLEMENTATION
#include "devfs.h"
#include "frame.h"
#include "hhdm.h"
#include "kprint.h"
#include "krlibc.h"
#include "page.h"
#include "rbtree-strptr.h"
#include "vdisk.h"
#include "vfs.h"

extern vdisk vdisk_ctl[26]; // vdisk.c

int                devfs_id   = 0;
static vfs_node_t  devfs_root = NULL;
static rbtree_sp_t dev_rbtree;

static int devfs_mount(const char *handle, vfs_node_t node) {
    if (handle != DEVFS_REGISTER_ID) return VFS_STATUS_FAILED;
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
    node->size = vdisk_ctl[(uint64_t)node->handle].type == VDISK_STREAM
                     ? (uint64_t)-1
                     : disk_size((int)(uint64_t)node->handle);
    node->realsize = vdisk_ctl[(uint64_t)node->handle].sector_size;
    return VFS_STATUS_SUCCESS;
}

static void devfs_open(void *parent, const char *name, vfs_node_t node) {
    if (node->type == file_dir) return;
    node->handle = rbtree_sp_get(dev_rbtree, name);
    node->type = vdisk_ctl[(uint64_t)node->handle].type == VDISK_STREAM ? file_stream : file_block;
    node->size = vdisk_ctl[(uint64_t)node->handle].type == VDISK_STREAM
                     ? (uint64_t)-1
                     : disk_size((int)(uint64_t)node->handle);
}

size_t devfs_read(void *file, void *addr, size_t offset, size_t size) {
    int    dev_id = (int)(uint64_t)file;
    size_t sector_size;
    size_t sectors_to_do;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    sector_size                      = vdisk_ctl[dev_id].sector_size;
    size_t padding_up_to_sector_size = PADDING_UP(size, sector_size);
    offset                           = PADDING_UP(offset, sector_size);

    if (vdisk_ctl[dev_id].type == VDISK_STREAM) goto read;
    if (offset > vdisk_ctl[dev_id].size) return VFS_STATUS_SUCCESS;
    if (vdisk_ctl[dev_id].size < offset + padding_up_to_sector_size) {
        // 计算需要读取的扇区数
        padding_up_to_sector_size = vdisk_ctl[dev_id].size - offset;
        if (size > padding_up_to_sector_size) { size = padding_up_to_sector_size; }
    }
read:

    sectors_to_do = padding_up_to_sector_size / sector_size;
    size_t page_size =
        (padding_up_to_sector_size / PAGE_SIZE) == 0 ? 1 : (padding_up_to_sector_size / PAGE_SIZE);
    uint64_t phys = alloc_frames(page_size);

    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, PTE_PRESENT | PTE_WRITEABLE);
    uint8_t *buffer0 = driver_phys_to_virt(phys);

    memset(buffer0, 0, size);

    size_t read_size = vdisk_read(offset / sector_size, sectors_to_do, buffer0, dev_id);
    memcpy(addr, buffer0, size);
    unmap_page_range(get_current_directory(), (uint64_t)buffer0, page_size * PAGE_SIZE);
    return read_size;
}

size_t devfs_write(void *file, const void *addr, size_t offset, size_t size) {
    int    dev_id = (int)(uint64_t)file;
    size_t sector_size;
    size_t sectors_to_do;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    sector_size                      = vdisk_ctl[dev_id].sector_size;
    size_t padding_up_to_sector_size = PADDING_UP(size, sector_size);
    offset                           = PADDING_UP(offset, sector_size);
    if (vdisk_ctl[dev_id].type == VDISK_STREAM) goto write;
    if (offset > vdisk_ctl[dev_id].size) return VFS_STATUS_SUCCESS;
    if (vdisk_ctl[dev_id].size < offset + padding_up_to_sector_size) {
        padding_up_to_sector_size = vdisk_ctl[dev_id].size - offset;
        if (size > padding_up_to_sector_size) { size = padding_up_to_sector_size; }
    }
write:
    sectors_to_do = padding_up_to_sector_size / sector_size;

    size_t page_size =
        (padding_up_to_sector_size / PAGE_SIZE) == 0 ? 1 : (padding_up_to_sector_size / PAGE_SIZE);
    uint64_t phys = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, PTE_PRESENT | PTE_WRITEABLE);
    uint8_t *buffer0 = driver_phys_to_virt(phys);

    if (padding_up_to_sector_size == size) {
    } else {
        vdisk_read(offset / sector_size, sectors_to_do, buffer0, dev_id);
    }
    memcpy(buffer0, addr, size);
    size_t ret_size = vdisk_write(offset / sector_size, sectors_to_do, buffer0, dev_id);
    unmap_page_range(get_current_directory(), (uint64_t)buffer0, page_size * PAGE_SIZE);
    return ret_size;
}

static int devfs_ioctl(void *file, size_t req, void *arg) {
    int dev_id = (int)(uint64_t)file;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    return vdisk_ctl[dev_id].ioctl(req, arg);
}

static vfs_node_t devfs_dup(vfs_node_t node) {
    vfs_node_t new_node = vfs_node_alloc(node->parent, node->name);
    if (new_node == NULL) return NULL;
    new_node->type        = node->type;
    new_node->handle      = node->handle;
    new_node->size        = node->size;
    new_node->child       = node->child;
    new_node->flags       = node->flags;
    new_node->permissions = node->permissions;
    new_node->realsize    = node->realsize;
    return new_node;
}

static int devfs_poll(void *file, size_t events) {
    int dev_id = (int)(uint64_t)file;
    if (vdisk_ctl[dev_id].flag == 0) return VFS_STATUS_FAILED;
    if (vdisk_ctl[dev_id].poll != (void *)empty) {
        return vdisk_ctl[dev_id].poll(events);
    } else
        return VFS_STATUS_SUCCESS;
}

static void *devfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot,
                       size_t flags) {
    int dev_id = (int)(uint64_t)file;
    if (vdisk_ctl[dev_id].flag == 0) return NULL;
    if (vdisk_ctl[dev_id].map) { return vdisk_ctl[dev_id].map(dev_id, addr, size); }
    return NULL;
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
    .ioctl   = devfs_ioctl,
    .dup     = devfs_dup,
    .delete  = (void *)empty,
    .rename  = (void *)empty,
    .poll    = devfs_poll,
    .map     = devfs_map,
};

void devfs_setup() {
    devfs_id = vfs_regist("devfs", &devfs_callbacks);
    vfs_mkdir("/dev");
    vfs_node_t dev = vfs_open("/dev");
    if (dev == NULL) {
        kerror("'dev' handle is null.");
        return;
    }
    if (vfs_mount(DEVFS_REGISTER_ID, dev) == VFS_STATUS_FAILED) {
        kerror("Cannot mount device file system.");
    }
}

void devfs_regist_dev(int drive_id) {
    if (have_vdisk(drive_id)) {
        vfs_child_append(devfs_root, vdisk_ctl[drive_id].drive_name, NULL);
        rbtree_sp_insert(dev_rbtree, vdisk_ctl[drive_id].drive_name, (void *)(uint64_t)drive_id);
    }
}
