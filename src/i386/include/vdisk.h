#pragma once

#include "ctypes.h"

typedef struct {
    void (*Read)(int drive, uint8_t *buffer, uint32_t number, uint32_t lba);
    void (*Write)(int drive, uint8_t *buffer, uint32_t number, uint32_t lba);
    int  flag;
    uint32_t  size;        // 大小
    uint32_t  sector_size; // 扇区大小
    char DriveName[50];
} vdisk;

void vdisk_init();
int register_vdisk(vdisk vd);
int logout_vdisk(int drive);
/*
 * 读取指定硬盘设备 (不推荐调用) 现已由devfs取代读取措施
 */
int rw_vdisk(int drive, uint32_t lba, uint8_t *buffer, uint32_t number, int read);
bool have_vdisk(int drive);
uint32_t disk_Size(int drive);
bool DiskReady(int drive);
void Disk_Write(uint32_t lba, uint32_t number, const void *buffer, int drive);
void Disk_Read(uint32_t lba, uint32_t number, void *buffer,int drive);
int getReadyDisk();