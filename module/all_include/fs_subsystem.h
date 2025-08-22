#pragma once

#include "cp_kernel.h"

typedef struct vfs_node *vfs_node_t;

typedef errno_t (*vfs_mount_t)(const char *src, vfs_node_t node);
typedef void (*vfs_unmount_t)(void *root);

typedef void (*vfs_open_t)(void *parent, const char *name, vfs_node_t node);
typedef void (*vfs_close_t)(void *current);
typedef void (*vfs_resize_t)(void *current, uint64_t size);

// 读写一个文件
typedef size_t (*vfs_write_t)(void *file, const void *addr, size_t offset, size_t size);
typedef size_t (*vfs_read_t)(void *file, void *addr, size_t offset, size_t size);
typedef size_t (*vfs_readlink_t)(vfs_node_t *node, void *addr, size_t offset, size_t size);

typedef errno_t (*vfs_stat_t)(void *file, vfs_node_t node);

// 创建一个文件或文件夹
typedef errno_t (*vfs_mk_t)(void *parent, const char *name, vfs_node_t node);
typedef errno_t (*vfs_del_t)(void *parent, vfs_node_t node);
typedef errno_t (*vfs_rename_t)(void *current, const char *new);

// VFS扩展接口 (CPOS特有, PLOS不支持)
typedef errno_t (*vfs_ioctl_t)(void *file, size_t req, void *arg);
typedef vfs_node_t (*vfs_dup_t)(vfs_node_t node);
typedef errno_t (*vfs_poll_t)(void *file, size_t events);
typedef void *(*vfs_mapfile_t)(void *file, void *addr, size_t offset, size_t size, size_t prot,
                               size_t flags);

enum {
    file_none     = 0x0UL,    // 未获取信息
    file_dir      = 0x2UL,    // 文件夹
    file_block    = 0x4UL,    // 块设备，如硬盘
    file_stream   = 0x8UL,    // 流式设备，如终端
    file_symlink  = 0x16UL,   // 符号链接
    file_fbdev    = 0x32UL,   // 帧缓冲设备
    file_keyboard = 0x64UL,   // 键盘设备
    file_mouse    = 0x128UL,  // 鼠标设备
    file_pipe     = 0x256UL,  // 管道设备
    file_socket   = 0x512UL,  // 套接字设备
    file_epoll    = 0x1024UL, // epoll 设备
};

typedef struct vfs_callback { // VFS回调函数
    vfs_mount_t    mount;     // 挂载文件系统
    vfs_unmount_t  unmount;   // 卸载文件系统 (虚拟文件系统不支持卸载)
    vfs_open_t     open;      // 打开一个文件句柄
    vfs_close_t    close;     // 关闭一个文件句柄
    vfs_read_t     read;      // 读取文件
    vfs_write_t    write;     // 写入文件
    vfs_readlink_t readlink;  // 读取软链接
    vfs_mk_t       mkdir;     // 创建文件夹
    vfs_mk_t       mkfile;    // 创建文件
    vfs_mk_t       link;      // 创建硬链接
    vfs_mk_t       symlink;   // 创建软链接
    vfs_stat_t     stat;      // 检查文件状态信息
    vfs_ioctl_t    ioctl;     // I/O 控制接口 (仅 devfs 等特殊文件系统实现)
    vfs_dup_t      dup;       // 复制文件节点
    vfs_poll_t     poll;      // 轮询文件状态 (仅 devfs 等特殊文件系统实现)
    vfs_mapfile_t  map;       // 映射文件到内存 (仅 devfs 等特殊文件系统实现)
    vfs_del_t delete;         // 删除文件或文件夹
    vfs_rename_t rename;      // 重命名文件或文件夹
} *vfs_callback_t;

typedef struct list *list_t;
struct list {
    union {
        void *data;
        isize idata;
        usize udata;
    };
    list_t prev;
    list_t next;
};

struct vfs_node {           // vfs节点
    vfs_node_t parent;      // 父目录
    vfs_node_t linkto;      // 符号链接指向的节点
    char      *name;        // 名称
    char      *linkname;    // 符号链接名称
    uint64_t   realsize;    // 项目真实占用的空间 (可选)
    uint64_t   size;        // 文件大小或若是文件夹则填0
    uint64_t   createtime;  // 创建时间
    uint64_t   readtime;    // 最后读取时间
    uint64_t   writetime;   // 最后写入时间
    uint64_t   inode;       // 节点编号
    uint64_t   blksz;       // 块大小
    uint32_t   owner;       // 所有者
    uint32_t   group;       // 所有组
    uint32_t   permissions; // 权限
    uint16_t   type;        // 类型
    uint32_t   refcount;    // 引用计数
    uint16_t   mode;        // 模式
    uint16_t   fsid;        // 文件系统的 id
    void      *handle;      // 操作文件的句柄
    uint64_t   flags;       // 文件标志
    list_t     child;       // 子节点
    vfs_node_t root;        // 根目录
    bool       visited;     // 是否与具体文件系统同步
};

errno_t    vfs_mkdir(const char *name);
errno_t    vfs_mkfile(const char *name);
int        vfs_regist(const char *name, vfs_callback_t callback);
vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle);
vfs_node_t vfs_node_alloc(vfs_node_t parent, const char *name);
errno_t    vfs_close(vfs_node_t node);
void       vfs_free(vfs_node_t vfs);
vfs_node_t vfs_open(const char *str);
errno_t    vfs_ioctl(vfs_node_t device, size_t options, void *arg);
size_t     vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size);  // 读取节点数据
size_t     vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size); // 写入节点
void *general_map(vfs_read_t read_callback, void *file, uint64_t addr, uint64_t len, uint64_t prot,
                  uint64_t flags, uint64_t offset);
vfs_node_t get_rootdir();
errno_t    vfs_unmount(const char *path);
errno_t    vfs_mount(const char *src, vfs_node_t node);
