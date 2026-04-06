#include "fs/memfd.h"
#include "errno.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/task.h"
#include "term/klog.h"
#include "syscall.h"

static int memfd_fsid = 0;

static size_t memfd_read(void *data, void *buf, size_t offset, size_t len) {
    const struct memfd_ctx *ctx = data;
    const size_t avail          = ctx->len - offset;
    if (avail <= 0) {
        return 0;
    }
    const size_t copy_len = len < avail ? len : avail;
    memcpy(buf, ctx->data + offset, copy_len);
    return copy_len;
}

static size_t memfd_write(void *data, const void *buf, size_t offset, size_t len) {
    struct memfd_ctx *ctx = data;
    spin_lock(ctx->lock);
    if (offset + len > ctx->len) {
        const size_t new_size    = ctx->len * 2;
        const uint64_t phys_addr = alloc_frames(new_size / PAGE_SIZE);
        uint8_t *new_data        = phys_to_virt(phys_addr);
        if (!new_data) {
            spin_unlock(ctx->lock);
            return -ENOMEM;
        }
        page_map_range(
            get_kernel_pagedir(), (uint64_t)new_data, phys_addr, ctx->len, KERNEL_PTE_FLAGS
        );
        memcpy(new_data, ctx->data, ctx->len);
        unmap_page_range(get_kernel_pagedir(), (uint64_t)ctx->data, ctx->len);
        ctx->data = new_data;
        ctx->len  = new_size;
    }
    memcpy(ctx->data + offset, buf, len);
    ctx->node->size = ctx->len;
    spin_unlock(ctx->lock);
    return len;
}

static void *
memfd_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    if ((flags & MAP_TYPE) == MAP_PRIVATE) {
        return general_map(memfd_read, file, (uint64_t)addr, size, prot, flags, offset);
    }
    const struct memfd_ctx *ctx = file;
    page_map_range(
        get_current_task()->process->mm->directory,
        (uint64_t)addr,
        virt_to_phys((void *)ctx->data + offset),
        size,
        KERNEL_PTE_FLAGS
    );
    return addr;
}

bool memfd_close(void *handle) {
    struct memfd_ctx *ctx = handle;
    if (!ctx) {
        return true;
    }

    spin_lock(ctx->lock);

    unmap_page_range(get_kernel_pagedir(), (uint64_t)ctx->data, ctx->len);

    spin_unlock(ctx->lock);
    ctx->node->handle = NULL;
    free(ctx);
    return true;
}

errno_t memfd_stat(void *file, vfs_node_t node) {
    struct memfd_ctx *ctx = file;
    if (!ctx) {
        return -EINVAL;
    }
    node->size = ctx->len;
    return EOK;
}

errno_t memfd_free(void *handle) {
    if (!handle) {
        return EOK;
    }
    free(handle);
    return EOK;
}

syscall_(memfd_create, const char *name, const unsigned int flags) {
    if (flags & MFD_HUGETLB) {
        return -EINVAL;
    }

    if (flags & MFD_NOEXEC_SEAL || flags & MFD_EXEC) {
        return -EINVAL;
    }

    struct memfd_ctx *ctx = malloc(sizeof(struct memfd_ctx));
    if (!ctx) {
        return SYSCALL_FAULT_(ENOMEM);
    }
    strncpy(ctx->name, name, 63);
    ctx->name[63] = '\0';
    ctx->len      = PAGE_SIZE;

    const uint64_t phys_addr = alloc_frames(1);
    ctx->data                = phys_to_virt(phys_addr);
    page_map_range(
        get_kernel_pagedir(), (uint64_t)ctx->data, phys_addr, ctx->len, KERNEL_PTE_FLAGS
    );
    memset(ctx->data, 0, ctx->len);
    ctx->flags = flags;
    ctx->lock  = SPIN_INIT;

    const vfs_node_t node = vfs_node_alloc(NULL, NULL);
    node->type            = file_none;
    node->fsid            = memfd_fsid;
    node->handle          = ctx;
    node->refcount++;
    node->size = 0;
    ctx->node  = node;
    fd_t *fd   = calloc(1, sizeof(fd_t));
    if (!fd) {
        free(ctx);
        return SYSCALL_FAULT_(ENOMEM);
    }
    fd->flags = flags;
    fd->node  = node;
    return add_fd(get_current_task()->process->fdts, fd);
}

static struct vfs_callback memfd_callbacks = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .close    = memfd_close,
    .stat     = memfd_stat,
    .open     = (vfs_open_t)dummy,
    .read     = memfd_read,
    .write    = memfd_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = (vfs_dup_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .poll     = (vfs_poll_t)dummy,
    .map      = memfd_map,
    .free     = memfd_free,
    .chmod    = (vfs_chmod_t)dummy,
    .mknod    = (vfs_mknod_t)dummy,
};

void memfd_setup() {
    memfd_fsid = vfs_regist("memfdfs", &memfd_callbacks, 0x45564644, FS_VIRTUAL_FLAGS);
    if (memfd_fsid == -EINVAL) {
        kerror("eventfdfs regist error.");
    }
}
