#pragma once

/*
 * PlantOS Virtual File System
 * Copyright plos-clan by min0911Y & zhouzhihao & copi143
 */

#include "ctypes.h"
#include "list.h"

typedef struct vfs_node *vfs_node_t;

typedef int (*vfs_mount_t)(const char* src, vfs_node_t node);
typedef void (*vfs_unmount_t)(void *root);

typedef void (*vfs_open_t)(void *parent,const char* name, vfs_node_t node);
typedef void (*vfs_close_t)(void *current);
typedef void (*vfs_resize_t)(void *current, uint64_t size);

// 读写一个文件
typedef int (*vfs_write_t)(void *file, const void *addr, size_t offset, size_t size);
typedef int (*vfs_read_t)(void *file, void *addr, size_t offset, size_t size);

typedef int (*vfs_stat_t)(void *file, vfs_node_t node);

// 创建一个文件或文件夹
typedef int (*vfs_mk_t)(void *parent,const char* name, vfs_node_t node);

// 映射文件从 offset 开始的 size 大小
typedef void *(*vfs_mapfile_t)(void *file, size_t offset, size_t size);

enum {
    file_none,   // 未获取信息
    file_dir,    // 文件夹
    file_block,  // 块设备，如硬盘
    file_stream, // 流式设备，如终端
};

typedef struct vfs_callback { //VFS回调函数
    vfs_mount_t   mount;
    vfs_unmount_t unmount;
    vfs_open_t    open;
    vfs_close_t   close;
    vfs_read_t    read;
    vfs_write_t   write;
    vfs_mk_t      mkdir;
    vfs_mk_t      mkfile;
    vfs_stat_t    stat; // 保留备用
} *vfs_callback_t;

struct vfs_node { //vfs节点
    vfs_node_t parent;      // 父目录
    char      *name;        // 名称
    uint64_t  realsize;    // 项目真实占用的空间 (可选)
    uint64_t  size;        // 文件大小或若是文件夹则填0
    uint64_t  createtime;  // 创建时间
    uint64_t  readtime;    // 最后读取时间
    uint64_t  writetime;   // 最后写入时间
    uint32_t  owner;       // 所有者
    uint32_t  group;       // 所有组
    uint32_t  permissions; // 权限
    uint8_t   type;        // 类型
    uint16_t  fsid;        // 文件系统的 id
    void      *handle;     // 操作文件的句柄
    list_t     child;      //
    vfs_node_t root;       // 根目录
};

struct fd {
    void  *file;
    size_t offset;
    bool   readable;
    bool   writeable;
};

extern struct vfs_callback vfs_empty_callback;
extern vfs_node_t rootdir;

int vfs_mkdir(const char* name); //创建文件夹节点
int vfs_mkfile(const char* name);//创建文件节点
int vfs_regist(const char* name, vfs_callback_t callback); //注册文件系统
vfs_node_t vfs_child_append(vfs_node_t parent, const char* name, void *handle);
vfs_node_t vfs_node_alloc(vfs_node_t parent, const char* name);
int vfs_close(vfs_node_t node); //关闭已打开的节点
void vfs_free(vfs_node_t vfs);
vfs_node_t vfs_open(const char* str); //打开一个节点
vfs_node_t vfs_do_search(vfs_node_t dir, const char* name);
void vfs_free_child(vfs_node_t vfs);
int vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size); //读取节点数据
int vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size); //写入节点
int vfs_mount(const char* src, vfs_node_t node); //挂载指定设备至指定节点
int vfs_unmount(const char* path); // 卸载指定设备的挂载点
vfs_node_t get_rootdir(); //获取根节点
bool vfs_init();