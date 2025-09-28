#include <cp_kernel.h>
#include <ext.h>
#include <fs_subsystem.h>
#include <lock.h>
#include <lwext4/blockdev/vfs_dev.h>
#include <string.h>
#include <util.h>

static int ext_fsid = 0;

spin_t rwlock = SPIN_INIT;

extern char *vfs_get_fullpath(vfs_node_t);

int ext_mount(const char *src, vfs_node_t node) {
    spin_lock(rwlock);

    if (is_virtual_fs(src)) return -1;

    ext4_device_register(vfs_dev_get(), src);

    vfs_dev_name_set(src);

    char *fullpath = vfs_get_fullpath(node);
    int   ret      = ext4_mount(src, (const char *)fullpath, false);

    if (ret != 0) {
        ext4_device_unregister(src);
        free(fullpath);
        spin_unlock(rwlock);
        return -1;
    }

    ext4_dir *dir = malloc(sizeof(ext4_dir));
    ext4_dir_open(dir, (const char *)fullpath);

    free(fullpath);

    const ext4_direntry *entry;
    while ((entry = ext4_dir_entry_next(dir))) {
        if (!strcmp((const char *)entry->name, ".") || !strcmp((const char *)entry->name, ".."))
            continue;
        void *_match_ = NULL;
        for (list_t n = node->child; n; n = n->next) {
            void *_data_ = n->data;
            if (!strcmp((const char *)entry->name, ((vfs_node_t)_data_)->name)) {
                _match_ = _data_;
                break;
            }
        }
        if (_match_) continue;
        vfs_node_t child = vfs_child_append(node, (const char *)entry->name, NULL);
        child->inode     = (uint64_t)entry->inode;
        child->fsid      = ext_fsid;
        if (entry->inode_type == EXT4_DE_SYMLINK)
            child->type = file_symlink;
        else if (entry->inode_type == EXT4_DE_DIR)
            child->type = file_dir;
        else
            child->type = file_none;
    }

    ext4_dir_close(dir);

    ext_handle_t *handle = malloc(sizeof(ext_handle_t));
    handle->dir          = dir;
    handle->node         = node;

    node->inode  = EXT4_ROOT_INO;
    node->handle = handle;

    spin_unlock(rwlock);

    return ret;
}

void ext_unmount(void *root) {
    // TODO
}

void ext_open(void *parent, const char *name, vfs_node_t node) {
    spin_lock(rwlock);

    ext_handle_t *handle = malloc(sizeof(ext4_file));
    handle->node         = node;
    char *path           = vfs_get_fullpath(node);
    if (node->type & file_dir) {
        handle->dir = malloc(sizeof(ext4_dir));
        if (ext4_dir_open(handle->dir, (const char *)path) != EOK) {
            goto symlink;

            char buf[1024];
            strcpy(buf, path);
            char  *s    = strrchr(buf, '/');
            size_t rcnt = 0;
            ext4_readlink(path, s + 1, 1024 - (s - buf) - 1, &rcnt);
            (s + 1)[rcnt] = '\0';
            if (ext4_dir_open(handle->dir, buf) != EOK) return;
        }

        const ext4_direntry *entry;
        while ((entry = ext4_dir_entry_next(handle->dir))) {
            if (!strcmp((const char *)entry->name, ".") || !strcmp((const char *)entry->name, ".."))
                continue;

            void *_match_ = NULL;
            for (list_t n = node->child; n; n = n->next) {
                void *_data_ = n->data;
                if (!strcmp((const char *)entry->name, ((vfs_node_t)_data_)->name)) {
                    _match_ = _data_;
                    break;
                }
            }
            if (_match_) continue;
            vfs_node_t child = vfs_child_append(node, (const char *)entry->name, NULL);
            child->fsid      = ext_fsid;
            child->inode     = (uint64_t)entry->inode;
            if (entry->inode_type == EXT4_DE_SYMLINK)
                child->type = file_symlink;
            else if (entry->inode_type == EXT4_DE_DIR)
                child->type = file_dir;
            else
                child->type = file_none;
        }

        ext4_dir_entry_rewind(handle->dir);
    } else if (node->type & file_symlink) {
    symlink:
        char  *path = vfs_get_fullpath(node);
        char  *buf  = malloc(1024);
        size_t rcnt = 0;
        ext4_readlink((const char *)path, buf, 1024, &rcnt);
        free(path);
        buf[rcnt] = '\0';
        spin_unlock(rwlock);
        char *parent_path = vfs_get_fullpath(node->parent);
        char  p[256];
        if (!strcmp(parent_path, "/"))
            snprintf(p, 256, "/%s", buf);
        else
            snprintf(p, 256, "%s/%s", parent_path, buf);
        free(buf);
        free(parent_path);
        node->linkto = vfs_open(p);
        spin_lock(rwlock);
        if (node->linkto) {
            node->linkto->refcount++;
            node->size = node->linkto->size;

            ext_handle_t *target_handle = node->linkto->handle;
            handle->file                = target_handle->file;
        }
    } else {
        handle->file = malloc(sizeof(ext4_file));
        ext4_fopen(handle->file, (const char *)path, "r+b");
        node->size = ext4_fsize(handle->file);
    }

    uint32_t mode = 0;
    ext4_mode_get((const char *)path, &mode);
    node->mode   = mode;
    node->handle = handle;

    free(path);

    spin_unlock(rwlock);
}

bool ext_close(void *current) {
    ext_handle_t *handle = current;
    if (handle->node->type & file_dir)
        ext4_dir_close(handle->dir);
    else
        ext4_fclose(handle->file);
    free(current);
    return true;
}

ssize_t ext_write(void *file, const void *addr, size_t offset, size_t size) {
    spin_lock(rwlock);

    ssize_t       ret    = 0;
    ext_handle_t *handle = file;
    if (!handle || !handle->node || !handle->file) return -1;
    if (handle->node->size < offset) {
        char *zero_buf = malloc(offset - handle->node->size);
        memset(zero_buf, 0, offset - handle->node->size);
        ext4_fseek(handle->file, (int64_t)handle->node->size, (uint32_t)SEEK_SET);
        ext4_fwrite(handle->file, zero_buf, offset - handle->node->size, NULL);
        free(zero_buf);
    }
    ext4_fseek(handle->file, (int64_t)offset, (uint32_t)SEEK_SET);
    ext4_fwrite(handle->file, addr, size, (size_t *)&ret);
    handle->node->size = ext4_fsize(handle->file);

    spin_unlock(rwlock);

    return ret;
}

ssize_t ext_read(void *file, void *addr, size_t offset, size_t size) {
    spin_lock(rwlock);

    ssize_t       ret    = 0;
    ext_handle_t *handle = file;
    if (!handle || !handle->node || !handle->file) return -1;
    if (handle->node->type & file_symlink) {
        ext_handle_t *target_handle = handle->node->linkto->handle;
        if (target_handle) {
            ext4_fseek(target_handle->file, (int64_t)offset, (uint32_t)SEEK_SET);
            ext4_fread(target_handle->file, addr, size, (size_t *)&ret);
        } else {
            printk("%s: symlink doesn't has a target file\n", __func__);
        }
    } else {
        ext4_fseek(handle->file, (int64_t)offset, (uint32_t)SEEK_SET);
        ext4_fread(handle->file, addr, size, (size_t *)&ret);
    }
    spin_unlock(rwlock);
    return ret;
}

ssize_t ext_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    vfs_node_t linkto = node->linkto;

    if (!linkto) { return -ENOLINK; }

    char *current_path = vfs_get_fullpath(node);
    char *linkto_path  = vfs_get_fullpath(linkto);

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    rel_status status = calculate_relative_path(buf, current_path, linkto_path, sizeof(buf));

    free(current_path);
    free(linkto_path);

    int len = strnlen(buf, size);
    memcpy(addr, buf, len);
    return len;
}

int ext_mkfile(void *parent, const char *name, vfs_node_t node) {
    spin_lock(rwlock);

    char buf[1024];

    ext_handle_t *parent_handle = parent;
    char         *parent_path   = vfs_get_fullpath(parent_handle->node);
    if (!strcmp(parent_path, "/"))
        snprintf(buf, 1024, "/%s", name);
    else
        snprintf(buf, 1024, "%s/%s", parent_path, name);
    free(parent_path);

    ext4_file f;
    int       ret = ext4_fopen2(&f, (const char *)buf, O_CREAT);
    ext4_fclose(&f);

    ext4_mode_set(buf, 0700);

    spin_unlock(rwlock);

    return ret;
}

int ext_link(void *parent, const char *name, vfs_node_t node) {
    return 0;
}

int ext_symlink(void *parent, const char *name, vfs_node_t node) {
    spin_lock(rwlock);

    char *fullpath = vfs_get_fullpath(node);

    int ret = ext4_fsymlink(name, fullpath);

    ext4_mode_set(name, 0700);

    free(fullpath);

    spin_unlock(rwlock);

    return ret;
}

int ext_mkdir(void *parent, const char *name, vfs_node_t node) {
    spin_lock(rwlock);

    char buf[1024];

    ext_handle_t *parent_handle = parent;
    char         *parent_path   = vfs_get_fullpath(parent_handle->node);
    if (!strcmp(parent_path, "/"))
        snprintf(buf, 1024, "/%s", name);
    else
        snprintf(buf, 1024, "%s/%s", parent_path, name);
    free(parent_path);

    int ret = ext4_dir_mk((const char *)buf);

    ext4_mode_set(buf, 0700);

    spin_unlock(rwlock);

    return ret;
}

errno_t ext_delete(void *parent, vfs_node_t node) {
    spin_lock(rwlock);

    char *path = vfs_get_fullpath(node);
    int   ret  = ext4_fremove((const char *)path);
    free(path);

    spin_unlock(rwlock);

    return ret;
}

int ext_rename(void *current, const char *new) {
    spin_lock(rwlock);

    ext_handle_t *handle = current;
    char         *path   = vfs_get_fullpath(handle->node);
    int           ret    = ext4_frename((const char *)path, new);
    free(path);

    spin_unlock(rwlock);

    return ret;
}

int ext_stat(void *file, vfs_node_t node) {
    ext_handle_t *handle = file;
    if (handle->node->type & file_none) { handle->node->size = ext4_fsize(handle->file); }

    return 0;
}

int ext_ioctl(void *file, size_t cmd, void *arg) {
    return -EINVAL;
}

int ext_poll(void *file, size_t events) {
    return 0;
}

void *ext_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map((vfs_read_t)ext_read, file, (uint64_t)addr, size, prot, flags, offset);
}

vfs_node_t ext_dup(vfs_node_t node) {
    return node;
}

static struct vfs_callback callbacks = {
    .mount    = ext_mount,
    .unmount  = ext_unmount,
    .open     = ext_open,
    .close    = (vfs_close_t)ext_close,
    .read     = (vfs_read_t)ext_read,
    .write    = (vfs_write_t)ext_write,
    .readlink = (vfs_readlink_t)ext_readlink,
    .mkdir    = ext_mkdir,
    .mkfile   = ext_mkfile,
    .link     = ext_link,
    .symlink  = ext_symlink,
    .delete   = ext_delete,
    .rename   = (vfs_rename_t)ext_rename,
    .map      = (vfs_mapfile_t)ext_map,
    .stat     = ext_stat,
    .ioctl    = ext_ioctl,
    .poll     = ext_poll,
    .dup      = (vfs_dup_t)ext_dup,
};

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    ext_fsid = vfs_regist("ext3", &callbacks, 0);
    if (ext_fsid == -1) {
        printk("Cannot register extfs file system.\n");
        return -EFAULT;
    }
    return EOK;
}
