/*
 * Plants-OS VDiskDriver
 * Copyright by min0911.
 */

#include "../include/vdisk.h"
#include "../include/printf.h"

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
