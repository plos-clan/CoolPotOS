#include "vdisk.h"
#include "klog.h"
#include "krlibc.h"
#include "fifo8.h"

vdisk vdisk_ctl[26];

void vdisk_init() {
    for (int i = 0; i < 26; i++) {
        vdisk_ctl[i].flag = 0; // 设置为未使用
    }
    klogf(true,"VDisk interface initialize.\n");
}

int register_vdisk(vdisk vd) {
    for (int i = 0; i < 26; i++) {
        if (!vdisk_ctl[i].flag) {
            vdisk_ctl[i] = vd; // 找到了！
            return i;          // 注册成功，返回drive
        }
    }
    return 0; // 注册失败
}

int logout_vdisk(int drive) {
    int indx = drive;
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

int rw_vdisk(int drive, uint32_t lba, uint8_t *buffer, uint32_t number, int read) {
    int indx = drive;
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

bool have_vdisk(int drive) {
    int indx = drive;
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

static uint8_t *drive_name[16] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static struct FIFO8 drive_fifo[16];

static uint8_t drive_buf[16][256];

bool set_drive(uint8_t *name) {
    for (int i = 0; i != 16; i++) {
        if (drive_name[i] == NULL) {
            drive_name[i] = name;
            //cir_queue8_init(&drive_fifo[i], 256, drive_buf[i]);
            return true;
        }
    }
    return false;
}

uint32_t get_drive_code(uint8_t *name) {
    for (int i = 0; i != 16; i++) {
        if (strcmp((char *)drive_name[i], (char *)name) == 0) { return i; }
    }
    return 16;
}

bool drive_semaphore_take(uint32_t drive_code) {
    return true;
    /*
    if (drive_code >= 16) { return true; }
    cir_queue8_put(&drive_fifo[drive_code], get_tid(current_task()));
    // printk("FIFO: %d PUT: %d STATUS: %d\n", drive_code, Get_Tid(current_task()),
    //        fifo8_status(&drive_fifo[drive_code]));
    while (drive_buf[drive_code][drive_fifo[drive_code].head] != get_tid(current_task())) {
        // printk("Waiting....\n");
    }
    return true;
     */
}

void drive_semaphore_give(uint32_t drive_code) {
    return;
    /*
    if (drive_code >= 16) { return; }
    if (drive_buf[drive_code][drive_fifo[drive_code].head] != get_tid(current_task())) {
        // 暂时先不做处理 一般不会出现这种情况
        return;
    }
    cir_queue8_get(&drive_fifo[drive_code]);
     */
}

#define SECTORS_ONCE 8
void Disk_Read(uint32_t lba, uint32_t number, void *buffer, int drive) {
    if (have_vdisk(drive)) {
        if (drive_semaphore_take(get_drive_code((uint8_t *)"DISK_DRIVE"))) {
            // printk("*buffer(%d %d) = %02x\n",lba,number,*(uint8_t *)buffer);
            for (int i = 0; i < number; i += SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, (uint8_t *)((uint32_t)buffer + i * vdisk_ctl[drive].sector_size), sectors,
                         1);
            }
            drive_semaphore_give(get_drive_code((uint8_t *)"DISK_DRIVE"));
        }
    }
}

uint32_t disk_Size(int drive) {
    uint8_t drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1;
        return vdisk_ctl[indx].size;
    } else {
        return 0;
    }

    return 0;
}

bool DiskReady(int drive) {
    return have_vdisk(drive);
}

int getReadyDisk() {
    return 0;
}

void Disk_Write(uint32_t lba, uint32_t number, const void *buffer, int drive) {
    //  printk("%d\n",lba);
    if (have_vdisk(drive)) {
        if (drive_semaphore_take(get_drive_code((uint8_t *)"DISK_DRIVE"))) {
            // printk("*buffer(%d %d) = %02x\n",lba,number,*(uint8_t *)buffer);
            for (int i = 0; i < number; i += SECTORS_ONCE) {
                int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
                rw_vdisk(drive, lba + i, (uint8_t *)((uint32_t)buffer + i * vdisk_ctl[drive].sector_size), sectors,
                         0);
            }
            drive_semaphore_give(get_drive_code((uint8_t *)"DISK_DRIVE"));
        }
    }
}