#pragma once

#define SECTORS_ONCE 8

#include "ctype.h"

typedef enum {
    VDISK_BLOCK,
    VDISK_STREAM,
} vdisk_flag_t;

typedef struct {
    void (*read)(int drive, uint8_t *buffer, uint32_t number, uint32_t lba);
    void (*write)(int drive, uint8_t *buffer, uint32_t number, uint32_t lba);
    int          flag;
    uint32_t     size;        // 大小
    uint32_t     sector_size; // 扇区大小
    vdisk_flag_t type;
    char         drive_name[50];
} vdisk; // 块设备

void     build_stream_device();
uint32_t disk_size(int drive);
int      vdisk_init();
int      regist_vdisk(vdisk vd);
bool     have_vdisk(int drive);
void     vdisk_read(uint32_t lba, uint32_t number, void *buffer, int drive);
void     vdisk_write(uint32_t lba, uint32_t number, const void *buffer, int drive);
