/*
 * Plants-OS VDiskDriver
 * Copyright by min0911.
 */

#include "../include/vdisk.h"
#include "../include/printf.h"
#include "../include/fifo.h"
#include "../include/task.h"

int getReadyDisk();

vdisk vdisk_ctl[26];

int init_vdisk() {
    for (int i = 0; i < 26; i++) {
        vdisk_ctl[i].flag = 0; // 设置为未使用
    }
    klogf(true,"VDisk interface initialize.\n");
}

int register_vdisk(vdisk vd) {
    for (int i = 0; i < 26; i++) {
        if (!vdisk_ctl[i].flag) {
            vdisk_ctl[i] = vd; // 找到了！
            return i + ('A');  // 注册成功，返回drive
        }
    }
    printk("[vdisk]not found\n");
    return 0; // 注册失败
}
int logout_vdisk(char drive) {
    int indx = drive - ('A');
    if (indx > 26) {
        return 0; // 失败
    }
    if (vdisk_ctl[indx].flag) {
        vdisk_ctl[indx].flag = 0; // 设置为没有
        return 1;                 // 成功
    } else {
        return 0; // 失败
    }
}
int rw_vdisk(char drive, unsigned int lba, unsigned char *buffer,
             unsigned int number, int read) {
    int indx = drive - ('A');
    if (indx > 26) {
        return 0; // 失败
    }
    if (vdisk_ctl[indx].flag) {
        if (read) {
            vdisk_ctl[indx].Read(drive, buffer, number, lba);
        } else {
            vdisk_ctl[indx].Write(drive, buffer, number, lba);
        }
        return 1; // 成功
    } else {
        return 0; // 失败
    }
}
bool have_vdisk(char drive) {
    int indx = drive - 'A';
    // printk("drive=%c\n",drive);
    if (indx > 26) {
        return 0; // 失败
    }
    if (vdisk_ctl[indx].flag) {
        return 1; // 成功
    } else {
        return 0; // 失败
    }
}
// 基于vdisk的通用读写

static unsigned char *drive_name[16] = {NULL, NULL, NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL};
static struct FIFO8 drive_fifo[16];
static unsigned char drive_buf[16][256];
bool SetDrive(unsigned char *name) {
    for (int i = 0; i != 16; i++) {
        if (drive_name[i] == NULL) {
            drive_name[i] = name;
            fifo8_init(&drive_fifo[i], 256, drive_buf[i]);
            return true;
        }
    }
    return false;
}
unsigned int GetDriveCode(unsigned char *name) {
    for (int i = 0; i != 16; i++) {
        if (strcmp((char *)drive_name[i], (char *)name) == 0) {
            return i;
        }
    }
    return 16;
}

bool DriveSemaphoreTake(unsigned int drive_code) {
    return true;
}
void DriveSemaphoreGive(unsigned int drive_code) {
    return;
}

void Disk_Read2048(unsigned int lba, unsigned int number, void *buffer,
                   char drive){
    if(have_vdisk(drive)){
        if (DriveSemaphoreTake(GetDriveCode((unsigned char *)"DISK_DRIVE"))) {
            for (int i = 0; i < number; i+=SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, buffer + i * 2048, sectors, 1);
            }
            DriveSemaphoreGive(GetDriveCode((unsigned char *)"DISK_DRIVE"));
        }
    }
}

void Disk_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive) {
    if (have_vdisk(drive)) {
        int indx = drive - ('A');
        if(vdisk_ctl[indx].flag != 1) {
            memset(buffer,0,number*512);
            return;
        }
        if (DriveSemaphoreTake(GetDriveCode((unsigned char *)"DISK_DRIVE"))) {
            for (int i = 0; i < number; i += SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, buffer + i * 512, sectors, 1);
            }
            DriveSemaphoreGive(GetDriveCode((unsigned char *)"DISK_DRIVE"));
        }
    }
}
bool CDROM_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive) {
    if (have_vdisk(drive)) {
        int indx = drive - ('A');
        if(vdisk_ctl[indx].flag != 2) {
            return false;
        }
        if (DriveSemaphoreTake(GetDriveCode((unsigned char *)"DISK_DRIVE"))) {
            for (int i = 0; i < number; i += SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, buffer + i * 2048, sectors, 1);
            }
            DriveSemaphoreGive(GetDriveCode((unsigned char *)"DISK_DRIVE"));
        }
        return  true;
    }
    return false;
}
unsigned int disk_Size(char drive) {
    unsigned char drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1 - 'A';
        return vdisk_ctl[indx].size;
    } else {
        return 0;
    }

    return 0;
}
bool DiskReady(char drive) { return have_vdisk(drive); }
int getReadyDisk() { return 0; }
void Disk_Write(unsigned int lba, unsigned int number, void *buffer,
                char drive) {
//  printk("%d\n",lba);
    if (have_vdisk(drive)) {
        if (DriveSemaphoreTake(GetDriveCode((unsigned char *)"DISK_DRIVE"))) {
            // printk("*buffer(%d %d) = %02x\n",lba,number,*(unsigned char *)buffer);
            for (int i = 0; i < number; i += SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, buffer + i * 512, sectors, 0);
            }
            DriveSemaphoreGive(GetDriveCode((unsigned char *)"DISK_DRIVE"));
        }
    }
}
