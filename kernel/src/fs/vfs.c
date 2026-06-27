#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/slub.h"
#include "term/kprint.h"

/* ---- 全局状态 ---- */
static vfs_fd_table_t  *g_fd_table;          /* 内核文件描述符表 */
static vfs_dentry_t    *g_root_dentry;       /* 根 dentry */
static struct llist_header g_fs_types;        /* 已注册文件系统类型 */
static struct llist_header g_mounts;          /* 挂载点列表 */
static vfs_inode_t     *g_root_inode;        /* 根 inode (tmpfs) */
static spin_t            g_vfs_lock = SPIN_INIT;

/* ---- 内部：临时文件系统 (tmpfs) inode 实现 ---- */
typedef struct {
    char  *data;          /* 文件数据 */
    size_t capacity;      /* 分配容量 */
} tmpfs_private_t;

/* ---- 内部辅助函数 ---- */

static vfs_dentry_t *dentry_hash_lookup(vfs_dentry_t *parent, const char *name) {
    vfs_dentry_t *child, *tmp;
    llist_for_each(child, tmp, &parent->children, child_node) {
        if (strcmp(child->name, name) == 0)
            return child;
    }
    return NULL;
}

static vfs_mount_t *vfs_find_mount(vfs_dentry_t *dentry) {
    vfs_mount_t *mnt, *tmp;
    llist_for_each(mnt, tmp, &g_mounts, node) {
        if (mnt->mount_point == dentry)
            return mnt;
    }
    return NULL;
}

/* 路径分量解析: 返回下一个 '/' 前的分量长度，处理连续 '/' */
static int path_next_component(const char **path, char *buf, size_t buf_size) {
    const char *p = *path;
    while (*p == '/') p++;
    if (*p == '\0') return 0;

    const char *start = p;
    while (*p && *p != '/') p++;
    size_t len = p - start;
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    *path = p;
    return (int)len;
}

/* ---- tmpfs 内部操作 ---- */

static vfs_inode_t *tmpfs_lookup(vfs_inode_t *dir, const char *name) {
    vfs_dentry_t *child, *tmp;
    llist_for_each(child, tmp, &dir->dentries, inode_node) {
        /* 遍历 dir 关联的所有 dentry */
    }
    /* 在子 dentry 中查找 */
    vfs_dentry_t *d, *t;
    /* 需要从 dentry 查找，这里通过 parent 关系 */
    /* 简化：遍历全局方式不够高效，但 tmpfs 场景用 dentry 即可 */
    return NULL;
}

static int tmpfs_create(vfs_inode_t *dir, const char *name, int type) {
    /* 创建新的 inode */
    vfs_inode_t *inode = vfs_inode_alloc(dir->sb);
    if (!inode) return -ENOMEM;

    inode->type = type;
    inode->mode = 0755;
    inode->nlink = (type == VFS_TYPE_DIR) ? 2 : 1;
    inode->size = 0;
    inode->private_data = NULL;

    /* 创建 dentry */
    vfs_dentry_t *parent_dentry = NULL;
    /* 从 dir 的 dentries 链表找到第一个 dentry 作为 parent */
    vfs_dentry_t *d, *tmp;
    llist_for_each(d, tmp, &dir->dentries, inode_node) {
        parent_dentry = d;
        break;
    }
    if (!parent_dentry && dir == g_root_inode) {
        parent_dentry = g_root_dentry;
    }

    vfs_dentry_t *child = vfs_dentry_alloc(parent_dentry, name);
    if (!child) {
        vfs_inode_release(inode);
        return -ENOMEM;
    }

    child->inode = inode;
    tmpfs_private_t *priv = calloc(1, sizeof(tmpfs_private_t));
    if (!priv && type != VFS_TYPE_DIR) {
        vfs_dentry_release(child);
        vfs_inode_release(inode);
        return -ENOMEM;
    }
    inode->private_data = priv;

    return 0;
}

static int tmpfs_unlink(vfs_inode_t *dir, const char *name) {
    /* 通过 dir 的 dentries 找到对应 child 并安全删除 */
    vfs_dentry_t *d, *tmp_d;
    llist_for_each(d, tmp_d, &dir->dentries, inode_node) {
        if (!d->parent) continue;
        /* 先收集要删除的 child，再统一删除，避免迭代器失效 */
        vfs_dentry_t *to_delete = NULL;
        vfs_dentry_t *child, *tmp_c;
        llist_for_each(child, tmp_c, &d->parent->children, child_node) {
            if (strcmp(child->name, name) == 0 && child->inode) {
                to_delete = child;
                break;
            }
        }
        if (!to_delete) continue;

        vfs_inode_t *inode = to_delete->inode;
        if (inode->type == VFS_TYPE_DIR && !llist_empty(&to_delete->children))
            return -ENOTEMPTY;

        to_delete->inode = NULL;
        if (inode->private_data) {
            tmpfs_private_t *priv = inode->private_data;
            if (priv->data) free(priv->data);
            free(priv);
        }
        vfs_dentry_release(to_delete);
        vfs_inode_release(inode);
        return 0;
    }
    return -ENOENT;
}

static int tmpfs_mkdir(vfs_inode_t *dir, const char *name) {
    return tmpfs_create(dir, name, VFS_TYPE_DIR);
}

static int tmpfs_rmdir(vfs_inode_t *dir, const char *name) {
    return tmpfs_unlink(dir, name);
}

static int tmpfs_truncate(vfs_inode_t *inode, size_t size) {
    if (!inode->private_data) {
        tmpfs_private_t *priv = calloc(1, sizeof(tmpfs_private_t));
        if (!priv) return -ENOMEM;
        inode->private_data = priv;
    }
    tmpfs_private_t *priv = inode->private_data;
    if (size > priv->capacity) {
        size_t new_cap = (size + 0xFFF) & ~0xFFF;
        void *new_data = realloc(priv->data, new_cap);
        if (!new_data) return -ENOMEM;
        memset((char *)new_data + priv->capacity, 0, new_cap - priv->capacity);
        priv->data = new_data;
        priv->capacity = new_cap;
    }
    inode->size = size;
    return 0;
}

static void tmpfs_release(vfs_inode_t *inode) {
    if (inode->private_data) {
        tmpfs_private_t *priv = inode->private_data;
        if (priv->data) free(priv->data);
        free(priv);
        inode->private_data = NULL;
    }
}

static vfs_inode_ops_t tmpfs_inode_ops = {
    .lookup   = tmpfs_lookup,
    .create   = tmpfs_create,
    .unlink   = tmpfs_unlink,
    .mkdir    = tmpfs_mkdir,
    .rmdir    = tmpfs_rmdir,
    .truncate = tmpfs_truncate,
    .release  = tmpfs_release,
};

/* ---- tmpfs 文件操作 ---- */

static ssize_t tmpfs_read(vfs_file_t *file, void *buf, size_t count) {
    vfs_inode_t *inode = file->dentry->inode;
    if (!inode || !inode->private_data) return 0;
    tmpfs_private_t *priv = inode->private_data;
    if (file->offset >= inode->size) return 0;
    size_t avail = inode->size - file->offset;
    if (count > avail) count = avail;
    if (priv->data)
        memcpy(buf, priv->data + file->offset, count);
    file->offset += count;
    return (ssize_t)count;
}

static ssize_t tmpfs_write(vfs_file_t *file, const void *buf, size_t count) {
    vfs_inode_t *inode = file->dentry->inode;
    if (!inode) return -EINVAL;
    if (file->offset + count > inode->size) {
        int ret = tmpfs_truncate(inode, file->offset + count);
        if (ret < 0) return ret;
    }
    tmpfs_private_t *priv = inode->private_data;
    if (priv && priv->data)
        memcpy(priv->data + file->offset, buf, count);
    file->offset += count;
    return (ssize_t)count;
}

static ssize_t tmpfs_ioctl(vfs_file_t *file, uint32_t cmd, uint64_t arg) {
    (void)file; (void)cmd; (void)arg;
    return -ENOTTY;
}

static int tmpfs_poll(vfs_file_t *file, int events) {
    (void)file;
    return events & (POLLIN | POLLOUT);
}

static vfs_file_ops_t tmpfs_file_ops = {
    .read  = tmpfs_read,
    .write = tmpfs_write,
    .ioctl = tmpfs_ioctl,
    .poll  = tmpfs_poll,
};

static vfs_file_ops_t tmpfs_dir_ops = {
    .read  = NULL,
    .write = NULL,
    .ioctl = tmpfs_ioctl,
    .poll  = tmpfs_poll,
};

/* ---- 超级块操作 ---- */

static vfs_inode_t *tmpfs_sb_alloc_inode(vfs_super_t *sb) {
    vfs_inode_t *inode = calloc(1, sizeof(vfs_inode_t));
    if (!inode) return NULL;
    inode->sb   = sb;
    inode->ops  = &tmpfs_inode_ops;
    inode->fops = &tmpfs_file_ops;
    inode->refcount = 1;
    llist_init_head(&inode->dentries);
    spin_lock(sb->lock);
    llist_append(&sb->inodes, &inode->node);
    spin_unlock(sb->lock);
    return inode;
}

static void tmpfs_sb_destroy_inode(vfs_inode_t *inode) {
    if (inode->ops && inode->ops->release)
        inode->ops->release(inode);
    llist_delete(&inode->node);
    free(inode);
}

static vfs_super_ops_t tmpfs_super_ops = {
    .alloc_inode   = tmpfs_sb_alloc_inode,
    .destroy_inode = tmpfs_sb_destroy_inode,
};

/* ---- tmpfs 挂载 ---- */

static vfs_super_t *tmpfs_mount(vfs_fs_type_t *fs_type, const char *dev, void *data) {
    (void)dev; (void)data;
    vfs_super_t *sb = calloc(1, sizeof(vfs_super_t));
    if (!sb) return NULL;
    sb->ops      = &tmpfs_super_ops;
    sb->fs_type  = fs_type;
    sb->dev      = 0;
    sb->block_size = PAGE_SIZE;
    sb->lock     = SPIN_INIT;
    llist_init_head(&sb->inodes);

    /* 创建根 inode */
    vfs_inode_t *root_inode = tmpfs_sb_alloc_inode(sb);
    if (!root_inode) { free(sb); return NULL; }
    root_inode->type  = VFS_TYPE_DIR;
    root_inode->inode_ops = &tmpfs_inode_ops;
    root_inode->fops = &tmpfs_dir_ops;
    root_inode->mode = 0755;
    root_inode->nlink = 2;

    /* 创建根 dentry */
    vfs_dentry_t *root_dentry = calloc(1, sizeof(vfs_dentry_t));
    if (!root_dentry) { tmpfs_sb_destroy_inode(root_inode); free(sb); return NULL; }
    strcpy(root_dentry->name, "/");
    root_dentry->inode    = root_inode;
    root_dentry->parent   = root_dentry; /* 根指向自己 */
    root_dentry->refcount = 1;
    llist_init_head(&root_dentry->children);
    llist_append(&root_inode->dentries, &root_dentry->inode_node);

    sb->root = root_dentry;
    return sb;
}

static vfs_fs_type_t tmpfs_type = {
    .name   = "tmpfs",
    .flags  = 0,
    .mount  = tmpfs_mount,
    .kill_sb = NULL,
};

/* ---- VFS 核心实现 ---- */

void vfs_init(void) {
    llist_init_head(&g_fs_types);
    llist_init_head(&g_mounts);

    /* 注册 tmpfs */
    vfs_register_fs(&tmpfs_type);

    /* 分配内核 FD 表 */
    g_fd_table = calloc(1, sizeof(vfs_fd_table_t));
    g_fd_table->lock     = SPIN_INIT;
    g_fd_table->refcount = 1;

    /* 挂载 tmpfs 为根文件系统 */
    vfs_mount_t *root_mnt = vfs_do_mount(NULL, "/", &tmpfs_type, NULL);
    if (!root_mnt) {
        kerror("VFS: failed to mount root filesystem");
        return;
    }

    g_root_dentry = root_mnt->root;
    g_root_inode  = root_mnt->root->inode;
    kinfo("VFS: root filesystem mounted (tmpfs)");
}

int vfs_register_fs(vfs_fs_type_t *fs) {
    if (!fs || !fs->name || !fs->mount) return -EINVAL;
    spin_lock(g_vfs_lock);
    llist_append(&g_fs_types, &fs->node);
    spin_unlock(g_vfs_lock);
    return 0;
}

int vfs_unregister_fs(vfs_fs_type_t *fs) {
    if (!fs) return -EINVAL;
    spin_lock(g_vfs_lock);
    llist_delete(&fs->node);
    spin_unlock(g_vfs_lock);
    return 0;
}

vfs_dentry_t *vfs_dentry_alloc(vfs_dentry_t *parent, const char *name) {
    vfs_dentry_t *dentry = calloc(1, sizeof(vfs_dentry_t));
    if (!dentry) return NULL;
    strncpy(dentry->name, name, VFS_NAME_MAX - 1);
    dentry->parent   = parent;
    dentry->refcount = 1;
    dentry->lock     = SPIN_INIT;
    llist_init_head(&dentry->children);
    if (parent) {
        llist_append(&parent->children, &dentry->child_node);
    }
    return dentry;
}

void vfs_dentry_release(vfs_dentry_t *dentry) {
    if (!dentry) return;
    llist_delete(&dentry->child_node);
    llist_delete(&dentry->inode_node);
    free(dentry);
}

vfs_inode_t *vfs_inode_alloc(vfs_super_t *sb) {
    if (!sb || !sb->ops || !sb->ops->alloc_inode) return NULL;
    return sb->ops->alloc_inode(sb);
}

void vfs_inode_release(vfs_inode_t *inode) {
    if (!inode || !inode->sb || !inode->sb->ops || !inode->sb->ops->destroy_inode) return;
    inode->sb->ops->destroy_inode(inode);
}

vfs_mount_t *vfs_do_mount(const char *dev, const char *path,
                          vfs_fs_type_t *fs_type, void *data) {
    if (!fs_type || !path) return NULL;

    vfs_super_t *sb = fs_type->mount(fs_type, dev, data);
    if (!sb) return NULL;

    vfs_mount_t *mnt = calloc(1, sizeof(vfs_mount_t));
    if (!mnt) {
        if (fs_type->kill_sb) fs_type->kill_sb(sb);
        return NULL;
    }

    mnt->sb   = sb;
    mnt->root = sb->root;
    sb->mount = mnt;

    spin_lock(g_vfs_lock);
    llist_append(&g_mounts, &mnt->node);
    spin_unlock(g_vfs_lock);

    return mnt;
}

int vfs_do_umount(const char *path) {
    (void)path;
    /* TODO: 实现卸载逻辑 */
    return -ENOSYS;
}

vfs_dentry_t *vfs_lookup(const char *path) {
    if (!path || !g_root_dentry) return NULL;
    if (path[0] != '/') return NULL;

    vfs_dentry_t *current = g_root_dentry;
    const char *p = path;
    char component[VFS_NAME_MAX];

    while (path_next_component(&p, component, sizeof(component)) > 0) {
        /* 检查挂载点跳转 */
        vfs_mount_t *mnt = vfs_find_mount(current);
        if (mnt) current = mnt->root;

        vfs_dentry_t *child = dentry_hash_lookup(current, component);
        if (!child) return NULL;
        current = child;
    }

    /* 检查最终路径上的挂载点 */
    vfs_mount_t *mnt = vfs_find_mount(current);
    if (mnt) current = mnt->root;

    return current;
}

int vfs_create(const char *path, int type) {
    if (!path) return -EINVAL;

    /* 解析父目录路径 */
    char parent_path[VFS_MAX_PATH];
    const char *name = NULL;
    size_t len = strlen(path);
    if (len >= VFS_MAX_PATH) return -ENAMETOOLONG;

    memcpy(parent_path, path, len + 1);
    for (int i = (int)len - 1; i >= 0; i--) {
        if (parent_path[i] == '/') {
            parent_path[i] = '\0';
            name = path + i + 1;
            break;
        }
    }
    if (!name || *name == '\0') return -EINVAL;
    if (parent_path[0] == '\0') parent_path[0] = '/', parent_path[1] = '\0';

    vfs_dentry_t *parent_dentry = vfs_lookup(parent_path);
    if (!parent_dentry || !parent_dentry->inode) return -ENOENT;
    if (parent_dentry->inode->type != VFS_TYPE_DIR) return -ENOTDIR;

    vfs_inode_t *dir = parent_dentry->inode;
    if (!dir->ops || !dir->ops->create) return -ENOSYS;

    return dir->ops->create(dir, name, type);
}

int vfs_unlink(const char *path) {
    if (!path) return -EINVAL;
    char parent_path[VFS_MAX_PATH];
    const char *name = NULL;
    size_t len = strlen(path);
    if (len >= VFS_MAX_PATH) return -ENAMETOOLONG;

    memcpy(parent_path, path, len + 1);
    for (int i = (int)len - 1; i >= 0; i--) {
        if (parent_path[i] == '/') {
            parent_path[i] = '\0';
            name = path + i + 1;
            break;
        }
    }
    if (!name || *name == '\0') return -EINVAL;
    if (parent_path[0] == '\0') parent_path[0] = '/', parent_path[1] = '\0';

    vfs_dentry_t *parent = vfs_lookup(parent_path);
    if (!parent || !parent->inode) return -ENOENT;
    if (parent->inode->type != VFS_TYPE_DIR) return -ENOTDIR;

    vfs_inode_t *dir = parent->inode;
    if (!dir->ops || !dir->ops->unlink) return -ENOSYS;
    return dir->ops->unlink(dir, name);
}

int vfs_mkdir(const char *path) {
    return vfs_create(path, VFS_TYPE_DIR);
}

int vfs_rmdir(const char *path) {
    return vfs_unlink(path);
}

/* ---- 文件描述符操作 ---- */

int vfs_open(const char *path, int flags, int mode) {
    if (!path || !g_fd_table) return -EINVAL;
    (void)mode;

    vfs_dentry_t *dentry = vfs_lookup(path);
    bool create_if_missing = (flags & O_CREAT) != 0;

    if (!dentry && create_if_missing) {
        int ret = vfs_create(path, VFS_TYPE_FILE);
        if (ret < 0) return ret;
        dentry = vfs_lookup(path);
    }

    if (!dentry) return -ENOENT;
    if (!dentry->inode) return -ENOENT;

    vfs_file_t *file = calloc(1, sizeof(vfs_file_t));
    if (!file) return -ENOMEM;

    file->dentry   = dentry;
    file->flags    = flags;
    file->mode     = mode;
    file->refcount = 1;

    /* 选择文件操作 */
    if (dentry->inode->fops)
        file->ops = dentry->inode->fops;
    else if (dentry->inode->type == VFS_TYPE_DIR)
        file->ops = &tmpfs_dir_ops;

    /* 分配 FD */
    spin_lock(g_fd_table->lock);
    int fd = -1;
    for (int i = 0; i < VFS_MAX_FD; i++) {
        if (g_fd_table->files[i] == NULL) {
            g_fd_table->files[i] = file;
            fd = i;
            break;
        }
    }
    spin_unlock(g_fd_table->lock);

    if (fd < 0) {
        free(file);
        return -EMFILE;
    }

    return fd;
}

ssize_t vfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    vfs_file_t *file = g_fd_table->files[fd];
    if (!file) return -EBADF;
    if (!file->ops || !file->ops->read) return -ENOSYS;
    return file->ops->read(file, buf, count);
}

ssize_t vfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    vfs_file_t *file = g_fd_table->files[fd];
    if (!file) return -EBADF;
    if (!file->ops || !file->ops->write) return -ENOSYS;
    return file->ops->write(file, buf, count);
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    spin_lock(g_fd_table->lock);
    vfs_file_t *file = g_fd_table->files[fd];
    g_fd_table->files[fd] = NULL;
    spin_unlock(g_fd_table->lock);
    if (!file) return -EBADF;
    free(file);
    return 0;
}

int vfs_ioctl(int fd, uint32_t cmd, uint64_t arg) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    vfs_file_t *file = g_fd_table->files[fd];
    if (!file) return -EBADF;
    if (!file->ops || !file->ops->ioctl) return -ENOTTY;
    return file->ops->ioctl(file, cmd, arg);
}

ssize_t vfs_pread(int fd, void *buf, size_t count, uint64_t offset) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    vfs_file_t *file = g_fd_table->files[fd];
    if (!file) return -EBADF;
    if (!file->ops || !file->ops->read) return -ENOSYS;
    uint64_t old_off = file->offset;
    file->offset = offset;
    ssize_t ret = file->ops->read(file, buf, count);
    file->offset = old_off;
    return ret;
}

ssize_t vfs_pwrite(int fd, const void *buf, size_t count, uint64_t offset) {
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fd_table) return -EBADF;
    vfs_file_t *file = g_fd_table->files[fd];
    if (!file) return -EBADF;
    if (!file->ops || !file->ops->write) return -ENOSYS;
    uint64_t old_off = file->offset;
    file->offset = offset;
    ssize_t ret = file->ops->write(file, buf, count);
    file->offset = old_off;
    return ret;
}

vfs_inode_t *vfs_get_root_inode(void) {
    return g_root_inode;
}

vfs_dentry_t *vfs_get_root_dentry(void) {
    return g_root_dentry;
}