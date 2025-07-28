/**
 * Virtual File System Interface
 * 虚拟文件系统接口
 * Create by min0911Y & zhouzhihao & copi143
 */
#define ALL_IMPLEMENTATION

#include "vfs.h"
#include "errno.h"
#include "kprint.h"
#include "krlibc.h"
#include "list.h"
#include "pcb.h"
#include "pipefs.h"

vfs_node_t rootdir = NULL;

static void empty_func() {}

struct vfs_callback vfs_empty_callback;

vfs_callback_t fs_callbacks[256] = {
    [0] = &vfs_empty_callback,
};
static int fs_nextid = 1;

#define callbackof(node, _name_) (fs_callbacks[(node)->fsid]->_name_)

static inline char *pathtok(char **sp) {
    char *s = *sp, *e = *sp;
    if (*s == '\0') return NULL;
    for (; *e != '\0' && *e != '/'; e++) {}
    if (*e == '/') {
        *e  = '\0';
        *sp = e + 1;
    } else {
        *sp = e;
    }
    return s;
}

static inline void do_open(vfs_node_t file) {
    if (file->handle != NULL) {
        callbackof(file, stat)(file->handle, file);
    } else {
        callbackof(file, open)(file->parent->handle, file->name, file);
    }
}

static inline void do_update(vfs_node_t file) {
    // assert(file->fsid != 0 || file->type != file_none);
    if (file->type == file_none || file->handle == NULL) do_open(file);
    // assert(file->type != file_none);
}

vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle) {
    vfs_node_t node = vfs_node_alloc(parent, name);
    if (node == NULL) return NULL;
    node->handle = handle;
    return node;
}

static vfs_node_t vfs_child_find(vfs_node_t parent, const char *name) {
    return list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
}

int vfs_mkdir(const char *name) {
    if (name[0] != '/') return VFS_STATUS_FAILED;
    char      *path     = strdup(name + 1);
    char      *save_ptr = path;
    vfs_node_t current  = rootdir;

    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) {
            continue; // 当前目录，不需要操作
        } else if (streq(buf, "..")) {
            if (current->parent) { current = current->parent; }
            continue;
        }

        vfs_node_t child = vfs_child_find(current, buf);
        if (child == NULL) {
            // 创建新目录
            child       = vfs_node_alloc(current, buf);
            child->type = file_dir;
            callbackof(current, mkdir)(current->handle, buf, child);
        } else {
            do_update(child);
            if (child->type != file_dir) {
                free(path);
                return VFS_STATUS_FAILED;
            }
        }
        current = child;
    }

    free(path);
    return VFS_STATUS_SUCCESS;
}

int vfs_mkfile(const char *name) {
    if (name[0] != '/') return VFS_STATUS_FAILED;

    // 分离路径和文件名
    char *fullpath  = strdup(name);
    char *filename  = fullpath;
    char *lastslash = strrchr(fullpath, '/');

    if (lastslash == fullpath) {
        // 根目录下的文件
        filename   = fullpath + 1;
        *lastslash = '\0';
    } else if (lastslash) {
        *lastslash = '\0';
        filename   = lastslash + 1;
    }

    // 打开父目录
    vfs_node_t parent;
    if (lastslash == fullpath) {
        parent = rootdir;
    } else {
        parent = vfs_open(fullpath);
    }

    if (parent == NULL || parent->type != file_dir) { return VFS_STATUS_FAILED; }

    // 创建文件
    vfs_node_t node = vfs_child_append(parent, filename, NULL);
    node->type      = file_block;
    callbackof(parent, mkfile)(parent->handle, filename, node);
    free(fullpath);
    return VFS_STATUS_SUCCESS;
}

int vfs_delete(vfs_node_t node) {
    if (node == rootdir) return VFS_STATUS_FAILED;
    int res = callbackof(node, delete)(node->parent->handle, node);
    if (res < 0) return VFS_STATUS_FAILED;
    list_delete(node->parent->child, node);
    node->handle = NULL;
    vfs_free(node);
    return VFS_STATUS_SUCCESS;
}

int vfs_rename(vfs_node_t node, const char *new) {
    return callbackof(node, rename)(node->handle, new);
}

int vfs_regist(const char *name, vfs_callback_t callback) {
    if (callback == NULL) return VFS_STATUS_FAILED;
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        if (((void **)callback)[i] == NULL) return VFS_STATUS_FAILED;
    }
    int id = fs_nextid++;

    fs_callbacks[id] = callback;
    return id;
}

vfs_node_t vfs_do_search(vfs_node_t dir, const char *name) {
    return list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
}

vfs_node_t vfs_open(const char *str) {
    if (str == NULL) return NULL;
    if (str[0] != '/') return NULL;
    if (str[1] == '\0') return rootdir; // 根目录

    char *path = strdup(str + 1);
    if (path == NULL) return NULL;

    char      *save_ptr = path;
    vfs_node_t current  = rootdir;

    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) {
            // 当前目录，不需要操作
            continue;
        } else if (streq(buf, "..")) {
            // 父目录
            if (current->parent) { current = current->parent; }
            continue;
        }

        vfs_node_t next = vfs_child_find(current, buf);
        if (next == NULL) {
            free(path);
            return NULL;
        }

        do_update(next);
        current = next;
    }

    free(path);
    return current;
}

void vfs_update(vfs_node_t node) {
    do_update(node);
}

void vfs_deinit() {
    // 目前并不支持
}

fd_file_handle *fd_dup(fd_file_handle *src) {
    fd_file_handle *new = (fd_file_handle *)malloc(sizeof(fd_file_handle));
    not_null_assets(new, "fd_dup out of memory.");
    src->node->refcount++;
    new->node       = src->node;
    new->offset     = src->offset;
    new->flags      = src->flags;
    vfs_node_t node = new->node;
    if (node->type == file_pipe) {
        pipe_specific_t *spec = node->handle;
        pipe_info_t     *pipe = spec->info;
        if (spec->write) {
            pipe->write_fds++;
        } else {
            pipe->read_fds++;
        }
    }
    return new;
}

vfs_node_t get_rootdir() {
    return rootdir;
}

void *vfs_map(vfs_node_t node, uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
              uint64_t offset) {
    if (node == NULL) return NULL;
    if (node->type == file_dir) return NULL;
    return callbackof(node, map)(node->handle, (void *)addr, offset, len, prot, flags);
}

vfs_node_t vfs_node_alloc(vfs_node_t parent, const char *name) {
    vfs_node_t node = malloc(sizeof(struct vfs_node));
    not_null_assets(node, "vfs alloc null");
    if (node == NULL) return NULL;
    memset(node, 0, sizeof(struct vfs_node));
    node->parent   = parent;
    node->name     = name ? strdup(name) : NULL;
    node->type     = file_none;
    node->fsid     = parent ? parent->fsid : 0;
    node->root     = parent ? parent->root : node;
    node->refcount = 0;
    node->blksz    = PAGE_SIZE;
    node->mode     = 0777;
    if (parent) list_prepend(parent->child, node);
    return node;
}

int vfs_close(vfs_node_t node) {
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node == rootdir) return VFS_STATUS_SUCCESS;
    if (node->handle == NULL) return 0;
    if (node->refcount > 0) {
        node->refcount--;
    } else {
        callbackof(node, close)(node->handle);
        node->handle = NULL;
    }
    return VFS_STATUS_SUCCESS;
}

void vfs_free(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
    vfs_close(vfs);
    free(vfs->name);
    free(vfs);
}

void vfs_free_child(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
}

int vfs_mount(const char *src, vfs_node_t node) {
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node->type != file_dir) return VFS_STATUS_FAILED;
    for (int i = 1; i < fs_nextid; i++) {
        if (fs_callbacks[i]->mount(src, node) == 0) {
            node->fsid = i;
            node->root = node;
            return VFS_STATUS_SUCCESS;
        }
    }
    return VFS_STATUS_FAILED;
}

size_t vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    return callbackof(file, read)(file->handle, addr, offset, size);
}

size_t vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    return callbackof(file, write)(file->handle, addr, offset, size);
}

int vfs_ioctl(vfs_node_t device, size_t options, void *arg) {
    if (device == NULL) return VFS_STATUS_FAILED;
    do_update(device);
    if (device->type == file_dir) return VFS_STATUS_FAILED;
    return callbackof(device, ioctl)(device->handle, options, arg);
}

int vfs_poll(vfs_node_t node, size_t event) {
    do_update(node);
    if (node->type & file_dir) return -1;
    return callbackof(node, poll)(node->handle, event);
}

int vfs_unmount(const char *path) {
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node->type != file_dir) return VFS_STATUS_FAILED;
    if (node->fsid == 0) return VFS_STATUS_FAILED;
    if (node->parent) {
        vfs_node_t cur = node;
        node           = node->parent;
        if (cur->root == cur) {
            vfs_free_child(cur);
            callbackof(cur, unmount)(cur->handle);
            cur->fsid   = node->fsid; // 交给上级
            cur->root   = node->root;
            cur->handle = NULL;
            cur->child  = NULL;
            // cur->type   = file_none;
            if (cur->fsid) do_update(cur);
            return VFS_STATUS_SUCCESS;
        }
    }
    return VFS_STATUS_FAILED;
}

void *general_map(vfs_read_t read_callback, void *file, uint64_t addr, uint64_t len, uint64_t prot,
                  uint64_t flags, uint64_t offset) {
    pcb_t current_task        = get_current_task()->parent_group;
    current_task->mmap_start += (len + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    uint64_t pt_flags = PTE_USER | PTE_WRITEABLE | PTE_PRESENT;

    if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
    if (!(prot & PROT_EXEC)) pt_flags |= PTE_NO_EXECUTE;

    page_map_range_to_random(get_current_directory(), addr & (~(PAGE_SIZE - 1)),
                             (len + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)), pt_flags);

    ssize_t ret = read_callback(file, (void *)addr, offset, len);
    if (ret < 0) return (void *)-ENOMEM;

    return (void *)addr;
}

spin_t get_path_lock = SPIN_INIT;

// 使用请记得free掉返回的buff
char *vfs_get_fullpath(vfs_node_t node) {
    if (node == NULL) return NULL;
    int inital = 32;
    spin_lock(get_path_lock);
    vfs_node_t *nodes = (vfs_node_t *)malloc(sizeof(vfs_node_t) * inital);
    not_null_assets(nodes, "vfs_get_fullpath: null alloc.");
    int count = 0;
    for (vfs_node_t cur = node; cur; cur = cur->parent) {
        if (count >= inital) {
            inital *= 2;
            nodes   = (vfs_node_t *)realloc((void *)nodes, (size_t)(sizeof(vfs_node_t) * inital));
        }
        nodes[count++] = cur;
    }
    // 正常的路径都不应该超过这个数值
    char *buff = (char *)malloc(256);
    strcpy(buff, "/");
    for (int j = count - 1; j >= 0; j--) {
        if (nodes[j] == rootdir) continue;
        strcat(buff, nodes[j]->name);
        if (j != 0) strcat(buff, "/");
    }
    free(nodes);
    spin_unlock(get_path_lock);
    return buff;
}

char *at_resolve_pathname(int dirfd, char *pathname) {
    if (pathname[0] == '/') { // by absolute pathname
        return strdup(pathname);
    } else if (pathname[0] != '/') {
        if (dirfd == AT_FDCWD) { // relative to cwd
            return vfs_cwd_path_build(pathname);
        } else { // relative to dirfd, resolve accordingly
            vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, dirfd);
            if (!node) return NULL;
            if (node->type != file_dir) return NULL;

            char *dirname = vfs_get_fullpath(node);

            char *prefix = vfs_get_fullpath(node->root);

            int prefixLen   = strlen(prefix);
            int rootDirLen  = strlen(dirname);
            int pathnameLen = strlen(pathname) + 1;

            char *out = malloc(prefixLen + rootDirLen + 1 + pathnameLen + 1);

            memcpy(out, prefix, prefixLen);
            memcpy(&out[prefixLen], dirname, rootDirLen);
            out[prefixLen + rootDirLen] = '/';
            memcpy(&out[prefixLen + rootDirLen + 1], pathname, pathnameLen);

            free(dirname);
            free(prefix);

            return out;
        }
    }

    return NULL;
}

char *vfs_cwd_path_build(char *src) {
    char *s = src;
    char *path;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        path = pathacat(get_current_task()->parent_group->cwd, s);
    }
    char *normalized_path = normalize_path(path);
    free(path);
    return normalized_path;
}

bool vfs_init() {
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        ((void **)&vfs_empty_callback)[i] = (void *)empty_func;
    }

    rootdir       = vfs_node_alloc(NULL, NULL);
    rootdir->type = file_dir;
    kinfo("Virtual File System initialize.");
    return true;
}
