#pragma once

#define SECTORS_ONCE 8

#include "ctype.h"

typedef enum {
    DEVICE_BLOCK,
    DEVICE_STREAM,
    DEVICE_FB,
} device_flag_t;

typedef struct _device {
    size_t (*read_vbuf)(int drive, struct vecbuf *buffer, size_t number, size_t lba);
    size_t (*write_vbuf)(int drive, struct vecbuf *buffer, size_t number, size_t lba);
    size_t (*read)(int drive, uint8_t *buffer, size_t number, size_t lba);
    size_t (*write)(int drive, uint8_t *buffer, size_t number, size_t lba);
    int (*ioctl)(struct _device *device, size_t req, void *handle);
    int (*poll)(size_t events);
    void *(*map)(int drive, void *addr, uint64_t len);
    int           flag;
    size_t        size;        // 大小
    size_t        sector_size; // 扇区大小
    device_flag_t type;
    char          drive_name[50];
    size_t        vdiskid; // 设备号
    char         *path;    // 设备注册路径
} device_t;                // 设备句柄

size_t disk_size(int drive);
int    device_manager_init();

/**
 * 注册一个设备 (会被自动映射进 devfs)
 * @param vd 设备
 * @param path 注册路径(为NULL注册进 /dev 根)
 * @return 注册编号
 */
int regist_device(const char *path, device_t vd);

/**
 * 根据指定设备id查找设备
 * @param id 设备id
 * @return 为 NULL 则找不到设备
 */
device_t *get_device(size_t id);

errno_t devfs_register(const char *path, size_t id);
bool    have_vdisk(int drive);
size_t  device_read(size_t lba, size_t number, void *buffer, int drive);
size_t  device_write(size_t lba, size_t number, const void *buffer, int drive);
void   *device_mmap(int drive, void *addr, uint64_t len);
