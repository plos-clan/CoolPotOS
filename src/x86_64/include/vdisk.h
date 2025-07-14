#pragma once

#define SECTORS_ONCE 8

#include "ctype.h"

typedef enum {
    VDISK_BLOCK,
    VDISK_STREAM,
} vdisk_flag_t;

typedef struct {
    size_t (*read)(int drive, uint8_t *buffer, size_t number, size_t lba);
    size_t (*write)(int drive, uint8_t *buffer, size_t number, size_t lba);
    int (*ioctl)(size_t req, void *handle);
    int (*poll)(size_t events);
    void *(*map)(int drive, void *addr, uint64_t len);
    int          flag;
    size_t       size;        // 大小
    size_t       sector_size; // 扇区大小
    vdisk_flag_t type;
    char         drive_name[50];
} vdisk; // 块设备

size_t disk_size(int drive);
int    vdisk_init();

/**
 * 注册一个硬盘设备
 * @param vd 硬盘设备
 * @return 注册编号
 */
int    regist_vdisk(vdisk vd);
bool   have_vdisk(int drive);
size_t vdisk_read(size_t lba, size_t number, void *buffer, int drive);
size_t vdisk_write(size_t lba, size_t number, const void *buffer, int drive);
void  *device_mmap(int drive, void *addr, uint64_t len);
