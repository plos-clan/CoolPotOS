#include "squashfs.h"

static errno_t squashfs_id         = -1;
static sqfs_u64 squashfs_mount_dev = 1;
errno_t errno                      = EOK;

typedef struct {
    sqfs_file_t base;
    vfs_node_t device;
    char *name;
    sqfs_u64 size;
} squashfs_image_t;

typedef struct {
    sqfs_compressor_t base;
    sqfs_u16 id;
} squashfs_compressor_stub_t;

static void squashfs_handle_release(squashfs_handle_t *handle) {
    if (handle == NULL) {
        return;
    }

    sqfs_free(handle->inode);
    squashfs_mount_drop(handle->mount);
    free(handle);
}

static int squashfs_image_read_at(sqfs_file_t *base, sqfs_u64 offset, void *buffer, size_t size) {
    const squashfs_image_t *file = (squashfs_image_t *)base;
    const size_t ret             = vfs_read(file->device, buffer, offset, size);
    return ret == size ? 0 : SQFS_ERROR_IO;
}

static int
squashfs_image_write_at(sqfs_file_t *base, sqfs_u64 offset, const void *buffer, size_t size) {
    (void)base;
    (void)offset;
    (void)buffer;
    (void)size;
    return SQFS_ERROR_UNSUPPORTED;
}

static sqfs_u64 squashfs_image_get_size(const sqfs_file_t *base) {
    const squashfs_image_t *file = (const squashfs_image_t *)base;
    return file->size;
}

static int squashfs_image_truncate(sqfs_file_t *base, sqfs_u64 size) {
    (void)base;
    (void)size;
    return SQFS_ERROR_UNSUPPORTED;
}

static const char *squashfs_image_get_filename(sqfs_file_t *base) {
    squashfs_image_t *file = (squashfs_image_t *)base;
    return file->name;
}

static void squashfs_image_destroy(sqfs_object_t *obj) {
    squashfs_image_t *file = (squashfs_image_t *)obj;
    if (file->device != NULL) {
        vfs_close(file->device);
    }
    free(file->name);
    free(file);
}

static squashfs_image_t *squashfs_image_create(vfs_node_t device, const char *name) {

    if (device == NULL) {
        return NULL;
    }

    squashfs_image_t *file = calloc(1, sizeof(*file));
    if (file == NULL) {
        return NULL;
    }

    sqfs_object_init(file, squashfs_image_destroy, NULL);
    file->base.read_at      = squashfs_image_read_at;
    file->base.write_at     = squashfs_image_write_at;
    file->base.get_size     = squashfs_image_get_size;
    file->base.truncate     = squashfs_image_truncate;
    file->base.get_filename = squashfs_image_get_filename;
    file->device            = device;
    file->name              = strdup(name != NULL ? name : "<device>");
    file->size              = device->size;
    return file;
}

static void
squashfs_cmp_get_configuration(const sqfs_compressor_t *cmp, sqfs_compressor_config_t *cfg) {
    const squashfs_compressor_stub_t *stub = (const squashfs_compressor_stub_t *)cmp;
    memset(cfg, 0, sizeof(*cfg));
    cfg->id    = stub->id;
    cfg->flags = SQFS_COMP_FLAG_UNCOMPRESS;
}

static int squashfs_cmp_write_options(sqfs_compressor_t *cmp, sqfs_file_t *file) {
    (void)cmp;
    (void)file;
    return 0;
}

static int squashfs_cmp_read_options(sqfs_compressor_t *cmp, sqfs_file_t *file) {
    (void)cmp;
    (void)file;
    return 0;
}

static sqfs_s32 squashfs_cmp_do_block(
    sqfs_compressor_t *cmp,
    const sqfs_u8 *in,
    const sqfs_u32 size,
    sqfs_u8 *out,
    const sqfs_u32 outsize
) {
    static bool warned                     = false;
    const squashfs_compressor_stub_t *stub = (squashfs_compressor_stub_t *)cmp;
    (void)in;
    (void)size;
    (void)out;
    (void)outsize;
    if (!warned) {
        printk(
            "squashfs: compressed block encountered, compression id=%u is not implemented\n",
            stub->id
        );
        warned = true;
    }
    return SQFS_ERROR_UNSUPPORTED;
}

static void squashfs_cmp_destroy(sqfs_object_t *obj) {
    free(obj);
}

static sqfs_compressor_t *squashfs_cmp_create(const sqfs_u16 id) {
    squashfs_compressor_stub_t *stub = calloc(1, sizeof(*stub));
    if (stub == NULL) {
        return NULL;
    }

    sqfs_object_init(stub, squashfs_cmp_destroy, NULL);
    stub->base.get_configuration = squashfs_cmp_get_configuration;
    stub->base.write_options     = squashfs_cmp_write_options;
    stub->base.read_options      = squashfs_cmp_read_options;
    stub->base.do_block          = squashfs_cmp_do_block;
    stub->id                     = id;
    return (sqfs_compressor_t *)stub;
}

void squashfs_mount_grab(squashfs_mount_t *mount) {
    if (mount != NULL) {
        mount->refcount += 1;
    }
}

void squashfs_mount_drop(squashfs_mount_t *mount) {
    if (mount == NULL)
        return;

    if (mount->refcount > 1) {
        mount->refcount -= 1;
        return;
    }

    sqfs_drop(mount->data_reader);
    sqfs_drop(mount->dir_reader);
    sqfs_drop(mount->ids);
    sqfs_drop(mount->cmp);
    sqfs_drop(mount->image);
    free(mount);
}

int squashfs_open_inode(squashfs_mount_t *mount, sqfs_u64 inode_ref, sqfs_inode_generic_t **out) {
    return sqfs_dir_reader_get_inode(mount->dir_reader, inode_ref, out);
}

static bool squashfs_child_exists(vfs_node_t node, const char *name) {
    list_foreach(node->child, it) {
        const vfs_node_t child = it->data;
        if (strcmp(child->name, name) == 0) {
            return true;
        }
    }
    return false;
}

int squashfs_map_inode_type(sqfs_u16 type) {
    switch (type) {
    case SQFS_INODE_DIR:
    case SQFS_INODE_EXT_DIR:
        return file_dir;
    case SQFS_INODE_SLINK:
    case SQFS_INODE_EXT_SLINK:
        return file_symlink;
    case SQFS_INODE_FIFO:
    case SQFS_INODE_EXT_FIFO:
        return file_pipe;
    case SQFS_INODE_SOCKET:
    case SQFS_INODE_EXT_SOCKET:
        return file_socket;
    default:
        return file_none;
    }
}

static void squashfs_fill_identity(
    const vfs_node_t node, const sqfs_inode_generic_t *inode, const squashfs_mount_t *mount
) {
    sqfs_u32 uid  = 0;
    sqfs_u32 gid  = 0;
    sqfs_u64 size = 0;

    node->inode       = inode->base.inode_number;
    node->mode        = inode->base.mode;
    node->permissions = inode->base.mode & 0x0FFF;
    node->owner       = 0;
    node->group       = 0;
    node->createtime  = inode->base.mod_time;
    node->readtime    = inode->base.mod_time;
    node->writetime   = inode->base.mod_time;
    node->blksz       = mount->super.block_size;

    if (sqfs_id_table_index_to_id(mount->ids, inode->base.uid_idx, &uid) == 0) {
        node->owner = uid;
    }
    if (sqfs_id_table_index_to_id(mount->ids, inode->base.gid_idx, &gid) == 0) {
        node->group = gid;
    }
    if (sqfs_inode_get_file_size(inode, &size) == 0) {
        node->size = size;
    }
}

void squashfs_fill_node(vfs_node_t node, squashfs_handle_t *handle) {
    const char *target = NULL;
    size_t target_size = 0;

    if (node == NULL || handle == NULL || handle->inode == NULL)
        return;

    node->fsid     = squashfs_id;
    node->dev      = node->root ? node->root->dev : node->dev;
    node->type     = squashfs_map_inode_type(handle->inode->base.type);
    node->size     = 0;
    node->realsize = 0;
    node->rdev     = 0;

    squashfs_fill_identity(node, handle->inode, handle->mount);

    switch (handle->inode->base.type) {
    case SQFS_INODE_DIR:
    case SQFS_INODE_EXT_DIR:
        node->type = file_dir;
        node->size = 0;
        break;
    case SQFS_INODE_SLINK:
        node->type  = file_symlink;
        node->size  = handle->inode->data.slink.target_size;
        target      = (const char *)handle->inode->extra;
        target_size = handle->inode->data.slink.target_size;
        break;
    case SQFS_INODE_EXT_SLINK:
        node->type  = file_symlink;
        node->size  = handle->inode->data.slink_ext.target_size;
        target      = (const char *)handle->inode->extra;
        target_size = handle->inode->data.slink_ext.target_size;
        break;
    case SQFS_INODE_BDEV:
    case SQFS_INODE_CDEV:
        node->type = file_block;
        node->rdev = handle->inode->data.dev.devno;
        break;
    case SQFS_INODE_EXT_BDEV:
    case SQFS_INODE_EXT_CDEV:
        node->type = file_block;
        node->rdev = handle->inode->data.dev_ext.devno;
        break;
    case SQFS_INODE_FIFO:
    case SQFS_INODE_EXT_FIFO:
        node->type = file_pipe;
        break;
    case SQFS_INODE_SOCKET:
    case SQFS_INODE_EXT_SOCKET:
        node->type = file_socket;
        break;
    default:
        node->type = file_none;
        break;
    }

    if (node->linkto_path != NULL) {
        free(node->linkto_path);
        node->linkto_path = NULL;
    }

    if (node->type == file_symlink && target != NULL) {
        node->linkto_path = strndup(target, target_size);
    }
}

int squashfs_populate_dir(const vfs_node_t node, const squashfs_handle_t *handle) {
    sqfs_dir_reader_state_t state;

    if (node == NULL || handle == NULL || handle->inode == NULL) {
        return 0;
    }
    if (node->type != file_dir) {
        return 0;
    }

    int ret = sqfs_dir_reader_open_dir(handle->mount->dir_reader, handle->inode, &state, 0);
    if (ret != 0) {
        return ret;
    }

    for (;;) {
        sqfs_dir_node_t *ent = NULL;
        ret                  = sqfs_dir_reader_read(handle->mount->dir_reader, &state, &ent);
        if (ret > 0) {
            break;
        }
        if (ret < 0)
            return ret;

        if (!squashfs_child_exists(node, (const char *)ent->name)) {
            const vfs_node_t child = vfs_child_append(node, (const char *)ent->name, NULL);
            if (child != NULL) {
                child->fsid    = squashfs_id;
                child->dev     = node->dev;
                child->type    = squashfs_map_inode_type(ent->type);
                child->visited = true;
            }
        }

        sqfs_free(ent);
    }

    return 0;
}

int squashfs_lookup_child(
    squashfs_mount_t *mount,
    const sqfs_inode_generic_t *parent,
    const char *name,
    sqfs_u64 *inode_ref,
    sqfs_inode_generic_t **out
) {
    sqfs_dir_reader_state_t state;

    if (inode_ref != NULL) {
        *inode_ref = 0;
    }
    if (out != NULL) {
        *out = NULL;
    }

    int ret = sqfs_dir_reader_open_dir(mount->dir_reader, parent, &state, 0);
    if (ret != 0) {
        return ret;
    }

    const size_t name_len = strlen(name);

    for (;;) {
        sqfs_dir_node_t *ent = NULL;
        ret                  = sqfs_dir_reader_read(mount->dir_reader, &state, &ent);
        if (ret != 0) {
            if (ret > 0) {
                return SQFS_ERROR_NO_ENTRY;
            }
            return ret;
        }

        if (ent->size + 1 == name_len && memcmp(ent->name, name, name_len) == 0) {
            if (inode_ref != NULL) {
                *inode_ref = state.ent_ref;
            }
            sqfs_free(ent);
            return out != NULL ? squashfs_open_inode(mount, state.ent_ref, out) : 0;
        }

        sqfs_free(ent);
    }
}

int squashfs_create_mount(
    const char *src, squashfs_mount_t **out, sqfs_inode_generic_t **root_inode, sqfs_u64 *root_ref
) {
    *out = NULL;
    if (root_inode != NULL) {
        *root_inode = NULL;
    }
    if (root_ref != NULL) {
        *root_ref = 0;
    }

    const vfs_node_t device = vfs_open(src);
    if (device == NULL || device->type == file_dir) {
        return -ENODEV;
    }

    squashfs_image_t *image = squashfs_image_create(device, src);
    if (image == NULL) {
        vfs_close(device);
        return -ENOMEM;
    }

    squashfs_mount_t *mount = calloc(1, sizeof(*mount));
    if (mount == NULL) {
        sqfs_drop(image);
        return -ENOMEM;
    }

    mount->refcount = 1;
    mount->image    = (sqfs_file_t *)image;

    int ret = sqfs_super_read(&mount->super, mount->image);
    if (ret != 0) {
        goto fail;
    }

    mount->cmp = squashfs_cmp_create(mount->super.compression_id);
    if (mount->cmp == NULL) {
        ret = SQFS_ERROR_ALLOC;
        goto fail;
    }

    mount->ids = sqfs_id_table_create(0);
    if (mount->ids == NULL) {
        ret = SQFS_ERROR_ALLOC;
        goto fail;
    }

    ret = sqfs_id_table_read(mount->ids, mount->image, &mount->super, mount->cmp);
    if (ret != 0)
        goto fail;

    mount->dir_reader = sqfs_dir_reader_create(&mount->super, mount->cmp, mount->image, 0);
    if (mount->dir_reader == NULL) {
        ret = SQFS_ERROR_ALLOC;
        goto fail;
    }

    mount->data_reader =
        sqfs_data_reader_create(mount->image, mount->super.block_size, mount->cmp, 0);
    if (mount->data_reader == NULL) {
        ret = SQFS_ERROR_ALLOC;
        goto fail;
    }

    ret = sqfs_data_reader_load_fragment_table(mount->data_reader, &mount->super);
    if (ret != 0) {
        goto fail;
    }

    if (root_ref != NULL) {
        *root_ref = mount->super.root_inode_ref;
    }
    if (root_inode != NULL) {
        ret = sqfs_dir_reader_get_root_inode(mount->dir_reader, root_inode);
        if (ret != 0) {
            goto fail;
        }
    }

    *out = mount;
    return 0;

fail:
    squashfs_mount_drop(mount);
    return ret;
}

static errno_t squashfs_error_to_errno(const int err) {
    switch (err) {
    case 0:
        return EOK;
    case SQFS_ERROR_NO_ENTRY:
        return -ENOENT;
    case SQFS_ERROR_NOT_DIR:
        return -ENOTDIR;
    case SQFS_ERROR_NOT_FILE:
        return -EISDIR;
    case SQFS_ERROR_UNSUPPORTED:
        return -ENOSYS;
    case SQFS_ERROR_ALLOC:
        return -ENOMEM;
    case SQFS_ERROR_IO:
        return -EIO;
    case SQFS_ERROR_OUT_OF_BOUNDS:
    case SQFS_ERROR_CORRUPTED:
    case SFQS_ERROR_SUPER_MAGIC:
    case SFQS_ERROR_SUPER_VERSION:
    case SQFS_ERROR_SUPER_BLOCK_SIZE:
        return -EINVAL;
    default:
        return -EINVAL;
    }
}

static squashfs_handle_t *squashfs_handle_create(
    squashfs_mount_t *mount, sqfs_inode_generic_t *inode, const sqfs_u64 inode_ref
) {
    squashfs_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        sqfs_free(inode);
        return NULL;
    }

    squashfs_mount_grab(mount);
    handle->mount     = mount;
    handle->inode     = inode;
    handle->inode_ref = inode_ref;
    return handle;
}

static void squashfs_open(void *parent, const char *name, vfs_node_t node) {
    const squashfs_handle_t *parent_handle = parent;
    sqfs_inode_generic_t *inode            = NULL;
    sqfs_u64 inode_ref                     = 0;

    if (parent_handle == NULL || node == NULL) {
        return;
    }

    int ret =
        squashfs_lookup_child(parent_handle->mount, parent_handle->inode, name, &inode_ref, &inode);
    if (ret != 0) {
        return;
    }

    squashfs_handle_t *handle = squashfs_handle_create(parent_handle->mount, inode, inode_ref);
    if (handle == NULL) {
        return;
    }

    node->handle = handle;
    squashfs_fill_node(node, handle);
    if (node->type == file_dir) {
        ret = squashfs_populate_dir(node, handle);
        if (ret != 0) {
            printk("squashfs: failed to populate directory %s: %d\n", name, ret);
            squashfs_handle_release(handle);
            node->handle = NULL;
        }
    }
}

static bool squashfs_close(void *current) {
    squashfs_handle_release(current);
    return true;
}

static size_t squashfs_read(void *file, void *addr, size_t offset, size_t size) {
    const squashfs_handle_t *handle = file;

    if (handle == NULL || addr == NULL) {
        return (size_t)-1;
    }

    if (handle->inode->base.type != SQFS_INODE_FILE
        && handle->inode->base.type != SQFS_INODE_EXT_FILE) {
        return (size_t)-1;
    }

    const sqfs_s32 ret =
        sqfs_data_reader_read(handle->mount->data_reader, handle->inode, offset, addr, size);
    return ret < 0 ? (size_t)-1 : (size_t)ret;
}

static size_t squashfs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)file;
    (void)addr;
    (void)offset;
    (void)size;
    return (size_t)-1;
}

static void *
squashfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map(squashfs_read, file, (uint64_t)addr, size, prot, flags, offset);
}

static size_t squashfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    const char *target;
    size_t target_size;

    if (node == NULL || addr == NULL) {
        return 0;
    }

    if (node->handle == NULL && node->parent != NULL && node->parent->handle != NULL) {
        squashfs_open(node->parent->handle, node->name, node);
    }

    const squashfs_handle_t *handle = node->handle;
    if (handle == NULL || handle->inode == NULL) {
        return 0;
    }

    if (handle->inode->base.type == SQFS_INODE_SLINK) {
        target      = (const char *)handle->inode->extra;
        target_size = handle->inode->data.slink.target_size;
    } else if (handle->inode->base.type == SQFS_INODE_EXT_SLINK) {
        target      = (const char *)handle->inode->extra;
        target_size = handle->inode->data.slink_ext.target_size;
    } else {
        return 0;
    }

    if (offset >= target_size) {
        return 0;
    }
    if (size > target_size - offset) {
        size = target_size - offset;
    }

    memcpy(addr, target + offset, size);
    return size;
}

static int squashfs_stat(void *file, vfs_node_t node) {
    squashfs_handle_t *handle = file;
    if (handle == NULL) {
        return -ENOENT;
    }

    squashfs_fill_node(node, handle);
    if (node->type == file_dir) {
        const int ret = squashfs_populate_dir(node, handle);
        if (ret != 0) {
            printk(
                "squashfs: stat populate failed for %s: %d\n", node->name ? node->name : "/", ret
            );
            return squashfs_error_to_errno(ret);
        }
    }
    return EOK;
}

static errno_t squashfs_rofs(void) {
    return -EROFS;
}

static int squashfs_dummy(void) {
    return -ENOSYS;
}

static errno_t squashfs_free_handle(void *handle) {
    squashfs_handle_release(handle);
    return EOK;
}

static errno_t squashfs_mount(const char *src, const vfs_node_t node, void *data) {
    squashfs_mount_t *mount;
    sqfs_inode_generic_t *root_inode = NULL;
    sqfs_u64 root_ref                = 0;

    (void)data;

    int ret = squashfs_create_mount(src, &mount, &root_inode, &root_ref);
    if (ret != 0) {
        printk("squashfs: create_mount failed: %d\n", ret);
        return squashfs_error_to_errno(ret);
    }

    squashfs_handle_t *handle = squashfs_handle_create(mount, root_inode, root_ref);
    if (handle == NULL) {
        printk("squashfs: failed to allocate root handle\n");
        squashfs_mount_drop(mount);
        return -ENOMEM;
    }

    node->dev    = squashfs_mount_dev++;
    node->handle = handle;
    node->fsid   = squashfs_id;
    node->root   = node;
    squashfs_fill_node(node, handle);
    ret = squashfs_populate_dir(node, handle);
    if (ret != 0) {
        printk("squashfs: mount failed while reading root directory: %d\n", ret);
        squashfs_handle_release(handle);
        node->handle = NULL;
        squashfs_mount_drop(mount);
        return squashfs_error_to_errno(ret);
    }
    squashfs_mount_drop(mount);
    return EOK;
}

static void squashfs_unmount(void *handle) {
    squashfs_handle_release(handle);
}

static struct vfs_callback callback = {
    .mount    = squashfs_mount,
    .unmount  = squashfs_unmount,
    .open     = squashfs_open,
    .close    = squashfs_close,
    .read     = squashfs_read,
    .write    = squashfs_write,
    .readlink = squashfs_readlink,
    .mkdir    = (vfs_mk_t)squashfs_rofs,
    .mkfile   = (vfs_mk_t)squashfs_rofs,
    .link     = (vfs_mk_t)squashfs_rofs,
    .symlink  = (vfs_mk_t)squashfs_rofs,
    .stat     = squashfs_stat,
    .ioctl    = (void *)squashfs_dummy,
    .dup      = (void *)squashfs_dummy,
    .poll     = (void *)squashfs_dummy,
    .map      = squashfs_map,
    .delete   = (void *)squashfs_rofs,
    .rename   = (void *)squashfs_rofs,
    .free     = squashfs_free_handle,
    .mknod    = (void *)squashfs_rofs,
    .chmod    = (void *)squashfs_rofs,
};

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    squashfs_id = vfs_regist("squashfs", &callback, 0x73717368, 0);
    if (squashfs_id == -EINVAL) {
        printk("Failed to register squash filesystem\n");
    }
    return EOK;
}
