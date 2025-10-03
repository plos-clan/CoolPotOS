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

#define SEEK_SET  0 /* Seek from beginning of file.  */
#define SEEK_CUR  1 /* Seek from current position.  */
#define SEEK_END  2 /* Seek from end of file.  */
#define SEEK_DATA 3
#define SEEK_HOLE 4

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

#include "../../../include/ctype.h"
#include "list.h"
#include "llist.h"

typedef struct vfs_node *vfs_node_t;

typedef errno_t (*vfs_mount_t)(const char *src, vfs_node_t node);
typedef void (*vfs_unmount_t)(void *root);

typedef void (*vfs_open_t)(void *parent, const char *name, vfs_node_t node);
typedef void (*vfs_close_t)(void *current);
typedef void (*vfs_resize_t)(void *current, uint64_t size);

// 读写一个文件
typedef size_t (*vfs_write_t)(void *file, const void *addr, size_t offset, size_t size);
typedef size_t (*vfs_read_t)(void *file, void *addr, size_t offset, size_t size);
typedef size_t (*vfs_readlink_t)(vfs_node_t node, void *addr, size_t offset, size_t size);

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
    file_none     = 0x1UL,    // 未获取信息
    file_dir      = 0x2UL,    // 文件夹
    file_block    = 0x4UL,    // 块设备，如硬盘
    file_stream   = 0x8UL,    // 流式设备，如终端
    file_symlink  = 0x10UL,   // 符号链接
    file_fbdev    = 0x20UL,   // 帧缓冲设备
    file_keyboard = 0x40UL,   // 键盘设备
    file_mouse    = 0x80UL,   // 鼠标设备
    file_pipe     = 0x100UL,  // 管道设备
    file_socket   = 0x200UL,  // 套接字设备
    file_epoll    = 0x400UL,  // epoll 设备
    file_ptmx     = 0x800UL,  // ptmx 设备
    file_pts      = 0x1000UL, // pts 设备
    file_proxy    = 0x2000UL, // 代理节点
    file_delete   = 0x4000UL, // 删除标记 (仅在删除时使用)
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

typedef struct vfs_filesystem {
    vfs_callback_t      callback;
    char                name[10];
    int                 id;
    uint16_t            fsid;
    uint64_t            magic;
    struct llist_header node;
} *vfs_filesystem_t;

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
    uint16_t   fsid;        // 文件系统挂载 id
    void      *handle;      // 操作文件的句柄
    uint64_t   flags;       // 文件标志
    list_t     child;       // 子节点
    vfs_node_t root;        // 根目录
    bool       visited;     // 是否与具体文件系统同步
    bool       is_mount;    // 是否是挂载点
    uint64_t   dev;         // 设备号
    uint64_t   rdev;        // 真实设备号
};

struct fd {
    void  *file;
    size_t offset;
    bool   readable;
    bool   writeable;
};

enum {
    DEVFS_REGISTER_ID = 0, // 设备文件系统注册ID
    MODFS_REGISTER_ID = 1, // 模块文件系统注册ID
    TMPFS_REGISTER_ID = 2, // 临时文件系统注册ID
    PIEFS_REGISTER_ID = 3, // 管道文件系统注册ID
    NETFS_REGISTER_ID = 4, // 网络文件系统注册ID
    CPFS_REGISTER_ID  = 5, // CPIO文件系统注册ID
    PROC_REGISTER_ID  = 6, // 进程信息文件系统注册ID
};

extern struct vfs_callback vfs_empty_callback;
extern vfs_node_t          rootdir;
extern struct llist_header fs_metadata_list;

/**
 * 创建目录节点
 * @param name 绝对路径
 * @return 非0代表创建失败
 */
errno_t vfs_mkdir(const char *name);

/**
 * 创建文件节点
 * @param name 绝对路径
 * @return 非0代表创建失败
 */
errno_t vfs_mkfile(const char *name);

/**
 * 注册文件系统回调指针
 * - 注意: 所有的回调函数都必须实现不得为NULL, 否则会注册失败.
 * (如果不需要某个回调函数, 用一个空实现替代即可)
 * @param name 文件系统名
 * @param callback 回调指针
 * @param register_id 文件系统挂载id (非虚拟文件系统填0)
 * @param magic 文件系统属性类型
 * @return 文件系统id
 */
int vfs_regist(const char *name, vfs_callback_t callback, int register_id, uint64_t magic);

/**
 * 创建 link 文件
 * @param name 被链接文件
 * @param target_name 创建的link文件名
 * @return 是否成功创建
 */
errno_t vfs_link(const char *name, const char *target_name);

/**
 * 创建一个符号链接文件
 * @param name 被链接文件
 * @param target_name 创建的链接文件
 * @return 是否成功创建
 */
errno_t vfs_symlink(const char *name, const char *target_name);

/**
 * 向父节点添加一个子节点
 * @param parent 父节点
 * @param name 子节点名称
 * @param handle 节点句柄
 * @return 子节点
 */
vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle);
vfs_node_t vfs_node_alloc(vfs_node_t parent, const char *name);

/**
 * 关闭一个已打开的 vfs 节点
 * @param node
 * @return 非0 代表错误返回
 */
errno_t vfs_close(vfs_node_t node);

/**
 * 释放一个 vfs 节点
 * @param vfs 节点
 */
void vfs_free(vfs_node_t vfs);

/**
 * 更新 vfs 节点的状态
 * @param node 节点
 */
void vfs_update(vfs_node_t node);

/**
 * 打开一个节点
 * @param str 路径(绝对路径)
 * @return 为NULL代表打开失败
 */
vfs_node_t vfs_open(const char *str);

/**
 * 向一个节点发送 I/O 控制命令
 * @param device 节点设备
 * @param options 操作码
 * @param arg 参数
 * @return 非0代表操作失败
 */
errno_t vfs_ioctl(vfs_node_t device, size_t options, void *arg);

/**
 * 读取一个符号链接文件
 * @param node 文件节点
 * @param buf 缓冲区
 * @param bufsize
 * @return
 */
size_t vfs_readlink(vfs_node_t node, char *buf, size_t bufsize);

/**
 * 根据类型获取文件系统句柄
 * @param type 类型
 * @return 句柄(NULL为找不到)
 */
vfs_filesystem_t get_filesystem(char *type);

/**
 * 获取指定文件节点的文件系统描述符
 * @param node 文件节点
 * @return 文件系统描述符
 */
vfs_filesystem_t get_filesystem_node(vfs_node_t node);

bool is_virtual_fs(const char *src);

vfs_node_t vfs_do_search(vfs_node_t dir, const char *name);
void       vfs_free_child(vfs_node_t vfs);
errno_t    vfs_delete(vfs_node_t node);
errno_t    vfs_rename(vfs_node_t node, const char *new);
errno_t    vfs_poll(vfs_node_t node, size_t event);
void      *vfs_map(vfs_node_t node, uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
                   uint64_t offset);
size_t     vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size);  // 读取节点数据
size_t     vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size); // 写入节点
void *general_map(vfs_read_t read_callback, void *file, uint64_t addr, uint64_t len, uint64_t prot,
                  uint64_t flags, uint64_t offset); // 文件映射

/**
 * 挂载一个文件系统到指定节点
  * 挂载后，src 代表设备的路径，node 代表挂载点
  * 挂载点必须是一个目录
  *
 * @param src 设备路径
 * @param node 挂载点
 * @return 非0代表挂载失败
 */
errno_t vfs_mount(const char *src, vfs_node_t node);

/**
 * 卸载一个挂载点
 * - 注意: 一些内核常驻挂载点不会被此函数卸载, 如 devfs/tmpfs 等
 * @param path 挂载点路径
 * @return 非0代表写在失败
 */
errno_t vfs_unmount(const char *path);

/**
 * 获取根目录节点
 * @return 根目录节点
 */
vfs_node_t get_rootdir();
void       set_rootdir(vfs_node_t node);

char *vfs_get_fullpath(vfs_node_t node);
char *at_resolve_pathname(int dirfd, char *pathname);
char *vfs_cwd_path_build(char *src); // 构建当前工作目录的路径
bool  vfs_init();
