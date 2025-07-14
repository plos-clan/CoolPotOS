#include "vdisk.h"
#include "devfs.h"

vdisk vdisk_ctl[26];

int regist_vdisk(vdisk vd) {
    for (int i = 0; i < 26; i++) {
        if (!vdisk_ctl[i].flag) {
            vdisk_ctl[i] = vd;
            devfs_regist_dev(i);
            return i;
        }
    }
    return 0;
}

bool have_vdisk(int drive) {
    int indx = drive;
    if (indx >= 26) { return false; }
    if (vdisk_ctl[indx].flag > 0) {
        return true;
    } else {
        return false;
    }
}

size_t disk_size(int drive) {
    uint8_t drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1;
        return vdisk_ctl[indx].size;
    } else {
        return 0;
    }
}

void *device_mmap(int drive, void *addr, uint64_t len) {
    if (have_vdisk(drive)) {
        int indx = drive;
        if (vdisk_ctl[indx].map) {
            return vdisk_ctl[indx].map(drive, addr, len);
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

size_t rw_vdisk(int drive, size_t lba, uint8_t *buffer, size_t number, int read) {
    int indx = drive;
    if (indx >= 26) { return 0; }
    if (vdisk_ctl[indx].flag > 0) {
        if (read) {
            return vdisk_ctl[indx].read(drive, buffer, number, lba);
        } else {
            return vdisk_ctl[indx].write(drive, buffer, number, lba);
        }
    } else {
        return 0;
    }
}

size_t vdisk_read(size_t lba, size_t number, void *buffer, int drive) {
    if (have_vdisk(drive)) {
        if (vdisk_ctl[drive].type == VDISK_STREAM) {
            return vdisk_ctl[drive].read(drive, buffer, number, lba);
        }
        size_t ret_size = 0;
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors  = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            ret_size    += rw_vdisk(drive, lba + i,
                                    (uint8_t *)((uint64_t)buffer + i * vdisk_ctl[drive].sector_size),
                                    sectors, 1);
        }
        return ret_size;
    } else
        return 0;
}

size_t vdisk_write(size_t lba, size_t number, const void *buffer, int drive) {
    if (have_vdisk(drive)) {
        size_t ret_size = 0;
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors  = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            ret_size    += rw_vdisk(drive, lba + i,
                                    (uint8_t *)((uint64_t)buffer + i * vdisk_ctl[drive].sector_size),
                                    sectors, 0);
        }
        return ret_size;
    } else
        return 0;
}

int vdisk_init() {
    for (size_t i = 0; i < 26; i++) {
        vdisk_ctl[i].flag = 0; // 设置为未使用
    }
    return 0;
}
