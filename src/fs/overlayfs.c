#define ALL_IMPLEMENTATION
#include "fs/overlayfs.h"
#include "errno.h"
#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/page.h"

static int overlayfs_id                  = 0;
static _Atomic uint64_t overlay_mount_id = 1;

typedef struct overlay_mount {
    size_t refcount;
    uint64_t dev;
    vfs_node_t upper_root;
    vfs_node_t lower_root;
} overlay_mount_t;

typedef struct overlay_handle {
    overlay_mount_t *mount;
    vfs_node_t upper_node;
    vfs_node_t lower_node;
    char *relpath;
} overlay_handle_t;

static vfs_node_t overlay_find_child(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }
    list_foreach(parent->child, it) {
        const vfs_node_t child = it->data;
        if (child != NULL && streq(child->name, name)) {
            return child;
        }
    }
    return NULL;
}

static char *overlay_join_path(const char *base, const char *tail) {
    if (base == NULL) {
        return NULL;
    }
    if (tail == NULL || tail[0] == '\0') {
        return strdup(base);
    }

    const size_t base_len = strlen(base);
    const size_t tail_len = strlen(tail);
    const bool need_slash = base_len == 0 || base[base_len - 1] != '/';
    char *out             = malloc(base_len + (need_slash ? 1 : 0) + tail_len + 1);
    if (out == NULL) {
        return NULL;
    }

    memcpy(out, base, base_len);
    size_t off = base_len;
    if (need_slash) {
        out[off++] = '/';
    }
    memcpy(out + off, tail, tail_len);
    out[off + tail_len] = '\0';
    return out;
}

static char *overlay_child_relpath(const char *parent_relpath, const char *name) {
    if (name == NULL) {
        return NULL;
    }
    if (parent_relpath == NULL || parent_relpath[0] == '\0') {
        return strdup(name);
    }
    return overlay_join_path(parent_relpath, name);
}

static void overlay_mount_grab(overlay_mount_t *mount) {
    if (mount != NULL) {
        mount->refcount++;
    }
}

static void overlay_rebind_tree(vfs_node_t node, vfs_node_t root) {
    if (node == NULL) {
        return;
    }
    node->root = root;
    list_foreach(node->child, it) {
        vfs_node_t child = it->data;
        if (child == NULL) {
            continue;
        }
        child->parent = node;
        overlay_rebind_tree(child, root);
    }
}

static void overlay_mount_drop(overlay_mount_t *mount) {
    if (mount == NULL) {
        return;
    }
    if (mount->refcount > 1) {
        mount->refcount--;
        return;
    }
    if (mount->lower_root != NULL) {
        vfs_free(mount->lower_root);
    }
    free(mount);
}

static overlay_handle_t *overlay_handle_create(
    overlay_mount_t *mount, const char *relpath, vfs_node_t upper_node, vfs_node_t lower_node
) {
    overlay_handle_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return NULL;
    }
    handle->mount      = mount;
    handle->upper_node = upper_node;
    handle->lower_node = lower_node;
    handle->relpath    = strdup(relpath != NULL ? relpath : "");
    if (handle->relpath == NULL) {
        free(handle);
        return NULL;
    }
    overlay_mount_grab(mount);
    return handle;
}

static errno_t overlay_free(void *handle_ptr) {
    overlay_handle_t *handle = handle_ptr;
    if (handle == NULL) {
        return EOK;
    }
    overlay_mount_drop(handle->mount);
    free(handle->relpath);
    free(handle);
    return EOK;
}

static void overlay_discard_wrapper(vfs_node_t node) {
    if (node == NULL) {
        return;
    }
    if (node->parent != NULL) {
        list_delete(node->parent->child, node);
    }
    if (node->handle != NULL) {
        overlay_free(node->handle);
    }
    if (node->linkto_path != NULL) {
        free(node->linkto_path);
    }
    if (node->linkname != NULL) {
        free(node->linkname);
    }
    if (node->name != NULL) {
        free(node->name);
    }
    free(node);
}

static vfs_node_t overlay_lookup_rel(vfs_node_t root, const char *relpath) {
    if (root == NULL) {
        return NULL;
    }

    vfs_update(root);
    if (relpath == NULL || relpath[0] == '\0') {
        return root;
    }

    char *path = strdup(relpath);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t current = root;
    char *cursor       = path;
    while (cursor != NULL && *cursor != '\0') {
        char *token = cursor;
        char *slash = strchr(cursor, '/');
        if (slash != NULL) {
            *slash  = '\0';
            cursor  = slash + 1;
        } else {
            cursor = NULL;
        }

        if (token[0] == '\0' || streq(token, ".")) {
            continue;
        }
        if (!streq(token, "..")) {
            vfs_update(current);
            current = overlay_find_child(current, token);
            if (current == NULL) {
                free(path);
                return NULL;
            }
            vfs_update(current);
            continue;
        }
        if (current->parent != NULL) {
            current = current->parent;
        }
    }

    free(path);
    return current;
}

static void overlay_refresh_binding(overlay_handle_t *handle) {
    if (handle == NULL || handle->mount == NULL) {
        return;
    }
    handle->upper_node = overlay_lookup_rel(handle->mount->upper_root, handle->relpath);
    handle->lower_node = overlay_lookup_rel(handle->mount->lower_root, handle->relpath);
}

static vfs_node_t overlay_effective_node(const overlay_handle_t *handle) {
    if (handle == NULL) {
        return NULL;
    }
    if (handle->upper_node != NULL) {
        return handle->upper_node;
    }
    return handle->lower_node;
}

static vfs_node_t overlay_meta_node(const overlay_handle_t *handle) {
    if (handle == NULL) {
        return NULL;
    }
    if (handle->upper_node != NULL && handle->lower_node != NULL
        && (handle->upper_node->type & file_dir) && (handle->lower_node->type & file_dir)) {
        return handle->lower_node;
    }
    return overlay_effective_node(handle);
}

static void overlay_sync_link(vfs_node_t node, vfs_node_t src) {
    node->linkto = NULL;
    if (node->linkto_path != NULL) {
        free(node->linkto_path);
        node->linkto_path = NULL;
    }
    if (src != NULL && src->linkto_path != NULL) {
        node->linkto_path = strdup(src->linkto_path);
    }
}

static void overlay_copy_metadata(vfs_node_t node, vfs_node_t src, overlay_mount_t *mount) {
    if (node == NULL || src == NULL || mount == NULL) {
        return;
    }
    node->type        = src->type;
    node->size        = src->size;
    node->realsize    = src->realsize;
    node->createtime  = src->createtime;
    node->readtime    = src->readtime;
    node->writetime   = src->writetime;
    node->blksz       = src->blksz;
    node->owner       = src->owner;
    node->group       = src->group;
    node->permissions = src->permissions;
    node->mode        = src->mode;
    node->rdev        = src->rdev;
    node->dev         = mount->dev;
    node->visited     = true;
    overlay_sync_link(node, src);
}

static errno_t overlay_ensure_upper_dir(overlay_handle_t *handle) {
    if (handle == NULL || handle->mount == NULL) {
        return -EINVAL;
    }
    overlay_refresh_binding(handle);
    if (handle->upper_node != NULL) {
        return (handle->upper_node->type & file_dir) ? EOK : -ENOTDIR;
    }

    char *path = vfs_get_fullpath(handle->mount->upper_root);
    if (path == NULL) {
        return -ENOMEM;
    }
    if (handle->relpath[0] != '\0') {
        char *joined = overlay_join_path(path, handle->relpath);
        free(path);
        path = joined;
    }
    if (path == NULL) {
        return -ENOMEM;
    }

    errno_t ret = vfs_mkdir(path);
    if (ret == EOK || ret == -EEXIST) {
        handle->upper_node = vfs_open(path);
        ret                = handle->upper_node != NULL ? EOK : -ENOENT;
    }
    free(path);
    return ret;
}

static char *overlay_build_upper_path(overlay_handle_t *handle) {
    if (handle == NULL || handle->mount == NULL) {
        return NULL;
    }
    char *path = vfs_get_fullpath(handle->mount->upper_root);
    if (path == NULL) {
        return NULL;
    }
    if (handle->relpath[0] == '\0') {
        return path;
    }
    char *joined = overlay_join_path(path, handle->relpath);
    free(path);
    return joined;
}

static errno_t overlay_copy_up(overlay_handle_t *handle) {
    if (handle == NULL || handle->mount == NULL) {
        return -EINVAL;
    }

    overlay_refresh_binding(handle);
    if (handle->upper_node != NULL) {
        return EOK;
    }
    if (handle->lower_node == NULL) {
        return -ENOENT;
    }

    vfs_node_t lower = handle->lower_node;
    if (lower->type & file_dir) {
        return overlay_ensure_upper_dir(handle);
    }

    char *path = overlay_build_upper_path(handle);
    if (path == NULL) {
        return -ENOMEM;
    }

    char *parent_path = get_parent_path(path);
    if (parent_path == NULL) {
        free(path);
        return -ENOMEM;
    }
    errno_t ret = vfs_mkdir(parent_path);
    free(parent_path);
    if (ret < 0 && ret != -EEXIST) {
        free(path);
        return ret;
    }

    if (lower->type & file_symlink) {
        const char *target = lower->linkto_path;
        if (target == NULL) {
            free(path);
            return -ENOSYS;
        }
        ret = vfs_symlink(path, target);
        if (ret < 0 && ret != -EEXIST) {
            free(path);
            return ret;
        }
    } else if (lower->type == file_none) {
        ret = vfs_mkfile(path);
        if (ret < 0 && ret != -EEXIST) {
            free(path);
            return ret;
        }
        handle->upper_node = vfs_open(path);
        if (handle->upper_node == NULL) {
            free(path);
            return -ENOENT;
        }

        char buffer[4096];
        size_t offset = 0;
        while (offset < lower->size) {
            size_t chunk = lower->size - offset;
            if (chunk > sizeof(buffer)) {
                chunk = sizeof(buffer);
            }
            size_t got = vfs_read(lower, buffer, offset, chunk);
            if (got == (size_t)-1) {
                free(path);
                return -EIO;
            }
            if (got == 0) {
                break;
            }
            size_t written = vfs_write(handle->upper_node, buffer, offset, got);
            if (written != got) {
                free(path);
                return -ENOSPC;
            }
            offset += got;
        }
    } else {
        free(path);
        return -ENOSYS;
    }

    if (handle->upper_node == NULL) {
        handle->upper_node = vfs_open(path);
    }
    if (handle->upper_node != NULL) {
        vfs_chmod(handle->upper_node, lower->mode);
    }
    free(path);
    return handle->upper_node != NULL ? EOK : -ENOENT;
}

static errno_t overlay_bind_wrapper(
    vfs_node_t node, overlay_mount_t *mount, const char *relpath, vfs_node_t upper, vfs_node_t lower
) {
    overlay_handle_t *handle = node->handle;
    if (handle == NULL) {
        handle = overlay_handle_create(mount, relpath, upper, lower);
        if (handle == NULL) {
            return -ENOMEM;
        }
        node->handle = handle;
    } else {
        handle->upper_node = upper;
        handle->lower_node = lower;
    }
    node->fsid = overlayfs_id;
    node->dev  = mount->dev;
    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_meta_node(handle);
    if (src == NULL) {
        return -ENOENT;
    }
    overlay_copy_metadata(node, src, mount);
    return EOK;
}

static errno_t overlay_refresh_dir_children(vfs_node_t node, overlay_handle_t *handle) {
    if (node == NULL || handle == NULL) {
        return -EINVAL;
    }

    if (handle->lower_node != NULL && (handle->lower_node->type & file_dir)) {
        vfs_update(handle->lower_node);
        list_foreach(handle->lower_node->child, it) {
            vfs_node_t lower_child = it->data;
            if (lower_child == NULL || lower_child->name == NULL) {
                continue;
            }
            vfs_node_t upper_child = NULL;
            if (handle->upper_node != NULL && (handle->upper_node->type & file_dir)) {
                vfs_update(handle->upper_node);
                upper_child = overlay_find_child(handle->upper_node, lower_child->name);
            }

            vfs_node_t child = overlay_find_child(node, lower_child->name);
            char *relpath    = overlay_child_relpath(handle->relpath, lower_child->name);
            if (relpath == NULL) {
                return -ENOMEM;
            }
            if (child == NULL) {
                child = vfs_node_alloc(node, lower_child->name);
                if (child == NULL) {
                    free(relpath);
                    return -ENOMEM;
                }
            }
            errno_t ret = overlay_bind_wrapper(child, handle->mount, relpath, upper_child, lower_child);
            free(relpath);
            if (ret < 0) {
                return ret;
            }
        }
    }

    if (handle->upper_node != NULL && (handle->upper_node->type & file_dir)) {
        vfs_update(handle->upper_node);
        list_foreach(handle->upper_node->child, it) {
            vfs_node_t upper_child = it->data;
            if (upper_child == NULL || upper_child->name == NULL) {
                continue;
            }
            vfs_node_t lower_child = NULL;
            if (handle->lower_node != NULL && (handle->lower_node->type & file_dir)) {
                lower_child = overlay_find_child(handle->lower_node, upper_child->name);
            }

            vfs_node_t child = overlay_find_child(node, upper_child->name);
            char *relpath    = overlay_child_relpath(handle->relpath, upper_child->name);
            if (relpath == NULL) {
                return -ENOMEM;
            }
            if (child == NULL) {
                child = vfs_node_alloc(node, upper_child->name);
                if (child == NULL) {
                    free(relpath);
                    return -ENOMEM;
                }
            }
            errno_t ret = overlay_bind_wrapper(child, handle->mount, relpath, upper_child, lower_child);
            free(relpath);
            if (ret < 0) {
                return ret;
            }
        }
    }

    return EOK;
}

static errno_t overlay_stat(void *file, vfs_node_t node) {
    overlay_handle_t *handle = file;
    if (handle == NULL || node == NULL) {
        return -ENOENT;
    }

    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_meta_node(handle);
    if (src == NULL) {
        return -ENOENT;
    }

    overlay_copy_metadata(node, src, handle->mount);
    if (node->type & file_dir) {
        return overlay_refresh_dir_children(node, handle);
    }
    return EOK;
}

static void overlay_open(void *parent, const char *name, vfs_node_t node) {
    overlay_handle_t *parent_handle = parent;
    if (parent_handle == NULL || name == NULL || node == NULL || node->handle != NULL) {
        return;
    }
    char *relpath = overlay_child_relpath(parent_handle->relpath, name);
    if (relpath == NULL) {
        return;
    }
    overlay_handle_t *handle =
        overlay_handle_create(parent_handle->mount, relpath, NULL, NULL);
    free(relpath);
    if (handle == NULL) {
        return;
    }
    overlay_refresh_binding(handle);
    if (overlay_effective_node(handle) == NULL) {
        overlay_free(handle);
        return;
    }
    node->handle = handle;
    overlay_stat(handle, node);
}

static bool overlay_close(void *current) {
    UNUSED(current);
    return false;
}

static size_t overlay_read(void *file, void *addr, size_t offset, size_t size) {
    overlay_handle_t *handle = file;
    if (handle == NULL) {
        return (size_t)-1;
    }
    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_effective_node(handle);
    if (src == NULL) {
        return (size_t)-1;
    }
    return vfs_read(src, addr, offset, size);
}

static size_t overlay_write(void *file, const void *addr, size_t offset, size_t size) {
    overlay_handle_t *handle = file;
    if (handle == NULL) {
        return (size_t)-1;
    }
    if (overlay_copy_up(handle) < 0 || handle->upper_node == NULL) {
        return (size_t)-1;
    }
    return vfs_write(handle->upper_node, (void *)addr, offset, size);
}

static size_t overlay_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    if (node == NULL || node->handle == NULL) {
        return 0;
    }
    overlay_handle_t *handle = node->handle;
    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_effective_node(handle);
    if (src == NULL || !(src->type & file_symlink)) {
        return 0;
    }
    if (offset != 0) {
        return 0;
    }
    return vfs_readlink(src, addr, size);
}

static errno_t overlay_mkdir(void *parent, const char *name, vfs_node_t node) {
    overlay_handle_t *parent_handle = parent;
    if (parent_handle == NULL || name == NULL || node == NULL) {
        return -EINVAL;
    }
    errno_t ret = overlay_ensure_upper_dir(parent_handle);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }

    char *relpath = overlay_child_relpath(parent_handle->relpath, name);
    if (relpath == NULL) {
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    overlay_handle_t temp = {
        .mount   = parent_handle->mount,
        .relpath = relpath,
    };
    char *path = overlay_build_upper_path(&temp);
    if (path == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    ret = vfs_mkdir(path);
    if (ret < 0 && ret != -EEXIST) {
        free(path);
        free(relpath);
        overlay_discard_wrapper(node);
        return ret;
    }

    vfs_node_t upper = vfs_open(path);
    free(path);
    if (upper == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOENT;
    }

    ret = overlay_bind_wrapper(node, parent_handle->mount, relpath, upper, NULL);
    free(relpath);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }
    return EOK;
}

static errno_t overlay_mkfile(void *parent, const char *name, vfs_node_t node) {
    overlay_handle_t *parent_handle = parent;
    if (parent_handle == NULL || name == NULL || node == NULL) {
        return -EINVAL;
    }
    errno_t ret = overlay_ensure_upper_dir(parent_handle);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }

    char *relpath = overlay_child_relpath(parent_handle->relpath, name);
    if (relpath == NULL) {
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    overlay_handle_t temp = {
        .mount   = parent_handle->mount,
        .relpath = relpath,
    };
    char *path = overlay_build_upper_path(&temp);
    if (path == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    ret = vfs_mkfile(path);
    if (ret < 0 && ret != -EEXIST) {
        free(path);
        free(relpath);
        overlay_discard_wrapper(node);
        return ret;
    }

    vfs_node_t upper = vfs_open(path);
    free(path);
    if (upper == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOENT;
    }

    ret = overlay_bind_wrapper(node, parent_handle->mount, relpath, upper, NULL);
    free(relpath);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }
    return EOK;
}

static errno_t overlay_link(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent, name);
    overlay_discard_wrapper(node);
    return -ENOSYS;
}

static errno_t overlay_symlink(void *parent, const char *name, vfs_node_t node) {
    overlay_handle_t *parent_handle = parent;
    if (parent_handle == NULL || name == NULL || node == NULL) {
        return -EINVAL;
    }
    errno_t ret = overlay_ensure_upper_dir(parent_handle);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }

    char *relpath = overlay_child_relpath(parent_handle->relpath, node->name);
    if (relpath == NULL) {
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }
    overlay_handle_t temp = {
        .mount   = parent_handle->mount,
        .relpath = relpath,
    };
    char *path = overlay_build_upper_path(&temp);
    if (path == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    ret = vfs_symlink(path, name);
    if (ret < 0 && ret != -EEXIST) {
        free(path);
        free(relpath);
        overlay_discard_wrapper(node);
        return ret;
    }

    vfs_node_t upper = vfs_open(path);
    free(path);
    if (upper == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOENT;
    }

    ret = overlay_bind_wrapper(node, parent_handle->mount, relpath, upper, NULL);
    free(relpath);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }
    return EOK;
}

static errno_t overlay_ioctl(void *file, size_t req, void *arg) {
    overlay_handle_t *handle = file;
    if (handle == NULL) {
        return -EINVAL;
    }
    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_effective_node(handle);
    if (src == NULL) {
        return -ENOENT;
    }
    return vfs_ioctl(src, req, arg);
}

static vfs_node_t overlay_dup(vfs_node_t node) {
    return node;
}

static errno_t overlay_poll(void *file, size_t events) {
    overlay_handle_t *handle = file;
    if (handle == NULL) {
        return -EINVAL;
    }
    overlay_refresh_binding(handle);
    vfs_node_t src = overlay_effective_node(handle);
    if (src == NULL) {
        return -ENOENT;
    }
    return vfs_poll(src, events);
}

static void *overlay_map(
    void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags
) {
    overlay_handle_t *handle = file;
    if (handle == NULL) {
        return (void *)-EINVAL;
    }
    if ((prot & PROT_WRITE) && overlay_copy_up(handle) < 0) {
        return (void *)-EIO;
    }
    overlay_refresh_binding(handle);
    vfs_node_t src = (prot & PROT_WRITE) ? handle->upper_node : overlay_effective_node(handle);
    if (src == NULL) {
        return (void *)-ENOENT;
    }
    return vfs_map(src, (uint64_t)addr, size, prot, flags, offset);
}

static errno_t overlay_delete(void *parent, vfs_node_t node) {
    UNUSED(parent, node);
    return -EROFS;
}

static errno_t overlay_rename(void *current, const char *new_name) {
    UNUSED(current, new_name);
    return -ENOSYS;
}

static errno_t overlay_mknod(void *parent, const char *name, vfs_node_t node, uint16_t mode, int dev) {
    overlay_handle_t *parent_handle = parent;
    if (parent_handle == NULL || name == NULL || node == NULL) {
        return -EINVAL;
    }
    errno_t ret = overlay_ensure_upper_dir(parent_handle);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }

    char *relpath = overlay_child_relpath(parent_handle->relpath, name);
    if (relpath == NULL) {
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }
    overlay_handle_t temp = {
        .mount   = parent_handle->mount,
        .relpath = relpath,
    };
    char *path = overlay_build_upper_path(&temp);
    if (path == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOMEM;
    }

    ret = vfs_mknod(path, mode, dev);
    if (ret < 0 && ret != -EEXIST) {
        free(path);
        free(relpath);
        overlay_discard_wrapper(node);
        return ret;
    }

    vfs_node_t upper = vfs_open(path);
    free(path);
    if (upper == NULL) {
        free(relpath);
        overlay_discard_wrapper(node);
        return -ENOENT;
    }

    ret = overlay_bind_wrapper(node, parent_handle->mount, relpath, upper, NULL);
    free(relpath);
    if (ret < 0) {
        overlay_discard_wrapper(node);
        return ret;
    }
    return EOK;
}

static errno_t overlay_chmod(vfs_node_t node, uint16_t mode) {
    if (node == NULL || node->handle == NULL) {
        return -EINVAL;
    }
    overlay_handle_t *handle = node->handle;
    errno_t ret              = overlay_copy_up(handle);
    if (ret < 0) {
        return ret;
    }
    if (handle->upper_node == NULL) {
        return -ENOENT;
    }
    return vfs_chmod(handle->upper_node, mode);
}

static vfs_node_t overlay_capture_lower_root(vfs_node_t node) {
    if (node == NULL) {
        return NULL;
    }

    vfs_node_t lower = calloc(1, sizeof(*lower));
    if (lower == NULL) {
        return NULL;
    }

    lower->name        = node->name == NULL ? NULL : strdup(node->name);
    lower->linkname    = node->linkname == NULL ? NULL : strdup(node->linkname);
    lower->linkto_path = node->linkto_path == NULL ? NULL : strdup(node->linkto_path);
    lower->linkto      = node->linkto;
    lower->realsize    = node->realsize;
    lower->size        = node->size;
    lower->createtime  = node->createtime;
    lower->readtime    = node->readtime;
    lower->writetime   = node->writetime;
    lower->inode       = node->inode;
    lower->blksz       = node->blksz;
    lower->owner       = node->owner;
    lower->group       = node->group;
    lower->permissions = node->permissions;
    lower->type        = node->type;
    lower->refcount    = 1;
    lower->mode        = node->mode;
    lower->fsid        = node->fsid;
    lower->handle      = node->handle;
    lower->flags       = node->flags;
    lower->child       = node->child;
    lower->root        = lower;
    lower->visited     = node->visited;
    lower->is_mount    = node->is_mount;
    lower->dev         = node->dev;
    lower->rdev        = node->rdev;
    lower->lock        = SPIN_INIT;
    lower->poll_waiters_lock = SPIN_INIT;
    llist_init_head(&lower->poll_waiters);

    overlay_rebind_tree(lower, lower);

    node->handle = NULL;
    node->child  = NULL;
    return lower;
}

static errno_t overlay_mount(const char *src, vfs_node_t node, void *data) {
    UNUSED(data);
    if (src == NULL || node == NULL) {
        return -EINVAL;
    }

    vfs_node_t upper_root = vfs_open(src);
    if (upper_root == NULL) {
        return -ENOENT;
    }
    vfs_update(upper_root);
    if (!(upper_root->type & file_dir)) {
        return -ENOTDIR;
    }

    overlay_mount_t *mount = calloc(1, sizeof(*mount));
    if (mount == NULL) {
        return -ENOMEM;
    }

    mount->upper_root = upper_root;
    mount->lower_root = overlay_capture_lower_root(node);
    mount->dev        = overlay_mount_id++;
    if (mount->lower_root == NULL) {
        free(mount);
        return -ENOMEM;
    }

    overlay_handle_t *root_handle =
        overlay_handle_create(mount, "", mount->upper_root, mount->lower_root);
    if (root_handle == NULL) {
        vfs_free(mount->lower_root);
        free(mount);
        return -ENOMEM;
    }

    node->handle = root_handle;
    node->type   = file_dir;
    node->fsid   = overlayfs_id;
    node->dev    = mount->dev;
    overlay_copy_metadata(node, overlay_meta_node(root_handle), mount);
    return EOK;
}

static void overlay_unmount(void *root) {
    overlay_free(root);
}

static struct vfs_callback overlay_callbacks = {
    .mount    = overlay_mount,
    .unmount  = overlay_unmount,
    .open     = overlay_open,
    .close    = overlay_close,
    .read     = overlay_read,
    .write    = overlay_write,
    .readlink = overlay_readlink,
    .mkdir    = overlay_mkdir,
    .mkfile   = overlay_mkfile,
    .link     = overlay_link,
    .symlink  = overlay_symlink,
    .stat     = overlay_stat,
    .ioctl    = overlay_ioctl,
    .dup      = overlay_dup,
    .poll     = overlay_poll,
    .map      = overlay_map,
    .delete   = overlay_delete,
    .rename   = overlay_rename,
    .free     = overlay_free,
    .mknod    = overlay_mknod,
    .chmod    = overlay_chmod,
};

void overlayfs_regist() {
    overlayfs_id = vfs_regist("overlayfs", &overlay_callbacks, 0x794C7630, FS_VIRTUAL_FLAGS);
}
