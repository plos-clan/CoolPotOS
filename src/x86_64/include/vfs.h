#pragma once

#define VFS_STATUS_FAILED  (-1)
#define VFS_STATUS_SUCCESS 0

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOGETCMAP         0x4604
#define FBIOPUTCMAP         0x4605
#define FBIOPAN_DISPLAY     0x4606

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETSIG        10
#define F_GETSIG        11
#define F_DUPFD_CLOEXEC 1030

#define SEEK_SET 0 /* Seek from beginning of file.  */
#define SEEK_CUR 1 /* Seek from current position.  */
#define SEEK_END 2 /* Seek from end of file.  */

#define FIOCLEX 0x5451

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

#define AT_FDCWD (-100)

#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_DSYNC     010000
#define O_SYNC      04010000
#define O_RSYNC     04010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000

#define O_ASYNC     020000
#define O_DIRECT    040000
#define O_LARGEFILE 0100000
#define O_NOATIME   01000000
#define O_PATH      010000000
#define O_TMPFILE   020200000
#define O_NDELAY    O_NONBLOCK

#define POLLIN  0x001 /* There is data to read.  */
#define POLLPRI 0x002 /* There is urgent data to read.  */
#define POLLOUT 0x004 /* Writing now will not block.  */

#include "ctype.h"
#include "list.h"

typedef struct vfs_node *vfs_node_t;

typedef int (*vfs_mount_t)(const char *src, vfs_node_t node);
typedef void (*vfs_unmount_t)(void *root);

typedef void (*vfs_open_t)(void *parent, const char *name, vfs_node_t node);
typedef void (*vfs_close_t)(void *current);
typedef void (*vfs_resize_t)(void *current, uint64_t size);

// 读写一个文件
typedef size_t (*vfs_write_t)(void *file, const void *addr, size_t offset, size_t size);
typedef size_t (*vfs_read_t)(void *file, void *addr, size_t offset, size_t size);

typedef int (*vfs_stat_t)(void *file, vfs_node_t node);

// 创建一个文件或文件夹
typedef int (*vfs_mk_t)(void *parent, const char *name, vfs_node_t node);
typedef int (*vfs_del_t)(void *parent, vfs_node_t node);
typedef int (*vfs_rename_t)(void *current, const char *new);

// VFS扩展接口 (CPOS特有, PLOS不支持)
typedef int (*vfs_ioctl_t)(void *file, size_t req, void *arg);
typedef vfs_node_t (*vfs_dup_t)(vfs_node_t node);
typedef int (*vfs_poll_t)(void *file, size_t events);
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
    vfs_mount_t   mount;      // 挂载文件系统
    vfs_unmount_t unmount;    // 卸载文件系统 (虚拟文件系统不支持卸载)
    vfs_open_t    open;       // 打开一个文件句柄
    vfs_close_t   close;      // 关闭一个文件句柄
    vfs_read_t    read;       // 读取文件
    vfs_write_t   write;      // 写入文件
    vfs_mk_t      mkdir;      // 创建文件夹
    vfs_mk_t      mkfile;     // 创建文件
    vfs_stat_t    stat;       // 检查文件状态信息
    vfs_ioctl_t   ioctl;      // I/O 控制接口 (仅 devfs 等特殊文件系统实现)
    vfs_dup_t     dup;        // 复制文件节点
    vfs_poll_t    poll;       // 轮询文件状态 (仅 devfs 等特殊文件系统实现)
    vfs_mapfile_t map;        // 映射文件到内存 (仅 devfs 等特殊文件系统实现)
    vfs_del_t delete;         // 删除文件或文件夹
    vfs_rename_t rename;      // 重命名文件或文件夹
} *vfs_callback_t;

struct vfs_node {           // vfs节点
    vfs_node_t parent;      // 父目录
    char      *name;        // 名称
    char      *linkname;    // 符号链接名称
    uint64_t   realsize;    // 项目真实占用的空间 (可选)
    uint64_t   size;        // 文件大小或若是文件夹则填0
    uint64_t   createtime;  // 创建时间
    uint64_t   readtime;    // 最后读取时间
    uint64_t   writetime;   // 最后写入时间
    uint32_t   owner;       // 所有者
    uint32_t   group;       // 所有组
    uint32_t   permissions; // 权限
    uint16_t   type;        // 类型
    uint16_t   fsid;        // 文件系统的 id
    void      *handle;      // 操作文件的句柄
    uint64_t   flags;       // 文件标志
    list_t     child;       // 子节点
    vfs_node_t root;        // 根目录
};

struct fd {
    void  *file;
    size_t offset;
    bool   readable;
    bool   writeable;
};

extern struct vfs_callback vfs_empty_callback;
extern vfs_node_t          rootdir;

int        vfs_mkdir(const char *name);                           // 创建文件夹节点
int        vfs_mkfile(const char *name);                          // 创建文件节点
int        vfs_regist(const char *name, vfs_callback_t callback); // 注册文件系统
vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle);
vfs_node_t vfs_node_alloc(vfs_node_t parent, const char *name);
vfs_node_t vfs_dup(vfs_node_t node);
int        vfs_close(vfs_node_t node); // 关闭已打开的节点
void       vfs_free(vfs_node_t vfs);
void       vfs_update(vfs_node_t node);
vfs_node_t vfs_open(const char *str); // 打开一个节点
int        vfs_ioctl(vfs_node_t device, size_t options, void *arg);
vfs_node_t vfs_do_search(vfs_node_t dir, const char *name);
void       vfs_free_child(vfs_node_t vfs);
int        vfs_delete(vfs_node_t node);
int        vfs_rename(vfs_node_t node, const char *new);
int        vfs_poll(vfs_node_t node, size_t event);
void      *vfs_map(vfs_node_t node, uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
                   uint64_t offset);
size_t     vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size);  // 读取节点数据
size_t     vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size); // 写入节点
void *general_map(vfs_read_t read_callback, void *file, uint64_t addr, uint64_t len, uint64_t prot,
                  uint64_t flags, uint64_t offset); // 文件映射
int   vfs_mount(const char *src, vfs_node_t node);  // 挂载指定设备至指定节点
int   vfs_unmount(const char *path);                // 卸载指定设备的挂载点
vfs_node_t get_rootdir();                           // 获取根节点
bool       vfs_init();
