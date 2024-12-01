#include "krlibc.h"
#include "kmalloc.h"
#include "klog.h"

#define ALL_IMPLEMENTATION

#include "vfs.h"
#include "list.h"


vfs_node_t rootdir = NULL;

static void empty_func() {}

struct vfs_callback vfs_empty_callback;

static vfs_callback_t fs_callbacks[256] = {
        [0] = &vfs_empty_callback,
};
static int fs_nextid = 1;

#define callbackof(node, _name_) (fs_callbacks[(node)->fsid]->_name_)

finline char *pathtok(char **_rest sp) {
    char *s = *sp, *e = *sp;
    if (*s == '\0') return NULL;
    for (; *e != '\0' && *e != '/'; e++) {}
    *sp = e + (*e != '\0' ? 1 : 0);
    *e  = '\0';
    return s;
}

finline void do_open(vfs_node_t file) {
    if (file->handle != NULL) {
        callbackof(file, stat)(file->handle, file);
    } else {
        callbackof(file, open)(file->parent->handle, file->name, file);
    }
}

finline void do_update(vfs_node_t file) {
    //assert(file->fsid != 0 || file->type != file_none);
    if (file->type == file_none || file->handle == NULL) do_open(file);
    //assert(file->type != file_none);
}

vfs_node_t vfs_child_append(vfs_node_t parent, const char* name, void *handle) {
    vfs_node_t node = vfs_node_alloc(parent, name);
    if (node == NULL) return NULL;
    node->handle = handle;
    return node;
}

static vfs_node_t vfs_child_find(vfs_node_t parent, const char* name) {
    return list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
}

int vfs_mkdir(const char* name) {
    if (name[0] != '/') return -1;
    char      *path     = strdup(name + 1);
    char      *save_ptr = path;
    vfs_node_t current  = rootdir;
    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        vfs_node_t father = current;
        if (streq(buf, ".")) {
            goto upd;
        } else if (streq(buf, "..")) {
            if (current->parent && current->type == file_dir) {
                current = current->parent;
                goto upd;
            } else {
                goto err;
            }
        }
        current = vfs_child_find(current, buf);

        upd:
        if (current == NULL) {
            current       = vfs_node_alloc(father, buf);
            current->type = file_dir;
            callbackof(father, mkdir)(father->handle, buf, current);
        } else {
            do_update(current);
            if (current->type != file_dir) goto err;
        }
    }

    kfree(path);
    return 0;

    err:
    kfree(path);
    return -1;
}

int vfs_mkfile(const char* name) {
    if (name[0] != '/') return -1;
    char      *path     = strdup(name + 1);
    char      *save_ptr = path;
    vfs_node_t current  = rootdir;
    char      *filename = path + strlen(path);

    while (*--filename != '/' && filename != path) {}
    *filename++ = '\0';
    if (strlen(path) == 0) {
        kfree(path);
        return -1;
    }
    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) {
            goto go;
        } else if (streq(buf, "..")) {
            if (current->parent && current->type == file_dir) {
                current = current->parent;
                goto go;
            } else {
                goto err;
            }
        }
        current = vfs_child_find(current, buf);
        go:
        if (current == NULL || current->type != file_dir) goto err;
    }

    vfs_node_t node = vfs_child_append(current, filename, NULL);
    node->type      = file_block;
    callbackof(current, mkfile)(current->handle, filename, node);

    kfree(path);
    return 0;

    err:
    kfree(path);
    return -1;
}

int vfs_regist(const char* name, vfs_callback_t callback) {
    if (callback == NULL) return -1;
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        if (((void **)callback)[i] == NULL) return -1;
    }
    int id           = fs_nextid++;
    fs_callbacks[id] = callback;
    return id;
}

vfs_node_t vfs_do_search(vfs_node_t dir, const char* name) {
    return list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
}

vfs_node_t vfs_open(const char* str) {
    if (str == NULL) return NULL;
    if (str[1] == '\0') return rootdir;
    char *path = strdup(str + 1);
    if (path == NULL) return NULL;

    char      *save_ptr = path;
    vfs_node_t current  = rootdir;
    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        vfs_node_t father = current;
        if (streq(buf, ".")) {
            goto upd;
        } else if (streq(buf, "..")) {
            if (current->parent && current->type == file_dir) {
                current = current->parent;
                goto upd;
            } else {
                goto err;
            }
        }
        current = vfs_child_find(current, buf);
        if (current == NULL) goto err;
        upd:
        do_update(current);
    }

    kfree(path);
    return current;

    err:
    kfree(path);
    return NULL;
}
void vfs_update(vfs_node_t node) {
    do_update(node);
}


void vfs_deinit() {
    // 目前并不支持
}

vfs_node_t get_rootdir(){
    return rootdir;
}

vfs_node_t vfs_node_alloc(vfs_node_t parent, const char* name) {
    vfs_node_t node = kmalloc(sizeof(struct vfs_node));
    if (node == NULL) return NULL;
    memset(node, 0, sizeof(struct vfs_node));
    node->parent = parent;
    node->name   = name ? strdup(name) : NULL;
    node->type   = file_none;
    node->fsid   = parent ? parent->fsid : 0;
    node->root   = parent ? parent->root : node;
    if (parent) list_prepend(parent->child, node);
    return node;
}
int vfs_close(vfs_node_t node) {
    if (node == NULL) return -1;
    if (node->handle == NULL) return 0;
    callbackof(node, close)(node->handle);
    node->handle = NULL;
    return 0;
}
void vfs_free(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
    vfs_close(vfs);
    kfree(vfs->name);
    kfree(vfs);
}
void vfs_free_child(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
}

int vfs_mount(const char* src, vfs_node_t node) {
    if (node == NULL) return -1;
    if (node->type != file_dir) return -1;
    void *handle = NULL;
    for (int i = 1; i < fs_nextid; i++) {
        // printf("trying %d", i);
        if (fs_callbacks[i]->mount(src, node) == 0) {
            node->fsid = i;
            node->root = node;
            return 0;
        }
    }
    return -1;
}

int vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
    do_update(file);
    if (file->type != file_block) return -1;
    return callbackof(file, read)(file->handle, addr, offset, size);
}
int vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size) {
    do_update(file);
    if (file->type != file_block) return -1;
    return callbackof(file, write)(file->handle, addr, offset, size);
}

int vfs_unmount(const char* path) {
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return -1;
    if (node->type != file_dir) return -1;
    if (node->fsid == 0) return -1;
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
            return 0;
        }
    }
    return -1;
}

bool vfs_init() {
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        ((void **)&vfs_empty_callback)[i] = empty_func;
    }

    rootdir       = vfs_node_alloc(NULL, NULL);
    rootdir->type = file_dir;
    klogf(true,"Virtual File System initialize.\n");
    return true;
}