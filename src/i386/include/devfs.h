#pragma once

#include "vfs.h"

int devfs_mount(const char* src, vfs_node_t node);
void devfs_regist();
int dev_get_sector_size(char *path);
int dev_get_size(char *path);
int dev_get_type(char *path); //1:HDD 2:CDROM
void print_devfs();
void devfs_sysinfo_init();