/*
 * Plants-OS VDiskDriver
 * Copyright by min0911.
 */

#include "../include/vdisk.h"
#include "../include/printf.h"
#include "../include/fifo.h"
#include "../include/task.h"

int getReadyDisk();

vdisk vdisk_ctl[10];

int init_vdisk() {
    for (int i = 0; i < 10; i++) {
        vdisk_ctl[i].flag = 0; // 设置为未使用
    }
    printf("[\035kernel\036]: VDisk driver load success!\n");
    return 0;
}

int register_vdisk(vdisk vd) {
    for (int i = 0; i < 10; i++) {
        if (!vdisk_ctl[i].flag) {
            vdisk_ctl[i] = vd; // 找到了！
            return i + ('A');  // 注册成功，返回drive
        }
    }
    printf("[vdisk]not found\n");
    return 0; // 注册失败
}

int logout_vdisk(char drive) {
    int indx = drive - ('A');
    if (indx > 10) {
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
    if (indx > 10) {
        return 0; // 失败
    }
    if (vdisk_ctl[indx].flag) {
        if (read) {
            vdisk_ctl[indx].Read(drive, buffer, number, lba);
            //for(;;);
        } else {
            vdisk_ctl[indx].Write(drive, buffer, number, lba);
        }
        return 1; // 成功
    } else {
        return 0; // 失败
    }
}

int have_vdisk(char drive) {
    int indx = drive - 'A';
    if (indx > 10) {
        return 0; // 失败
    }
    if (vdisk_ctl[indx].flag) {
        return 1; // 成功
    } else {
        return 0; // 失败
    }
}

void Disk_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive) {
    if (have_vdisk(drive)) {
        for (int i = 0; i < number; i += SECTORS_ONCE) {
            int sectors =
                    ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            rw_vdisk(drive, lba + i, buffer + i * 512, sectors, 1);
        }
    }
}

int disk_Size(char drive) {
    unsigned char drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1 - 'A';
        return vdisk_ctl[indx].size;
    } else {
        print("Disk Not Ready.\n");
        return 0;
    }

    return 0;
}

int DiskReady(char drive) { return have_vdisk(drive); }

int getReadyDisk() { return 0; }

void Disk_Write(unsigned int lba, unsigned int number, void *buffer,
                char drive) {
    if (have_vdisk(drive)) {
        for (int i = 0; i < number; i += SECTORS_ONCE) {
            int sectors =
                    ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            rw_vdisk(drive, lba + i, buffer + i * 512, sectors, 0);
        }
    }
}

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
            return 1;
        }
    }
    return 0;
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
    if (drive_code >= 16) {
        return 1;
    }
    fifo8_put(&drive_fifo[drive_code], get_current()->pid);
    // printk("FIFO: %d PUT: %d STATUS: %d\n", drive_code, Get_Tid(current_task()),
    //        fifo8_status(&drive_fifo[drive_code]));
    while (drive_buf[drive_code][drive_fifo[drive_code].q] !=
           get_current()->pid) {
        // printk("Waiting....\n");
    }
    return 1;
}
void DriveSemaphoreGive(unsigned int drive_code) {
    if (drive_code >= 16) {
        return;
    }
    if (drive_buf[drive_code][drive_fifo[drive_code].q] != get_current()->pid) {
        return;
    }
    fifo8_get(&drive_fifo[drive_code]);
}
