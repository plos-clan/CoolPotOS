#include "vdisk.h"
#include "devfs.h"
#include "krlibc.h"
#include "tty.h"
#include "pcb.h"
#include "keyboard.h"
#include "kprint.h"

vdisk vdisk_ctl[26];

static void stdin_read(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    UNUSED(lba);
    UNUSED(drive);
    for (size_t i = 0; i < number; i++) {
        char c = (char) kernel_getch();
        buffer[i] = c;
    }
}

static void stdout_write(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    tty_t *tty;
    UNUSED(drive);
    UNUSED(lba);
    if (get_current_task() == NULL) {
        tty = get_default_tty();
    } else tty = get_current_task()->tty;
    for (size_t i = 0; i < number; i++) {
        tty->putchar(tty, buffer[i]);
    }
}

void build_stream_device() {
    vdisk stdout;
    stdout.type = VDISK_STREAM;
    strcpy(stdout.drive_name, "stdout");
    stdout.flag = 1;
    stdout.sector_size = 1;
    stdout.size = 1;
    stdout.read = (void*)empty;
    stdout.write = stdout_write;
    regist_vdisk(stdout);

    vdisk stdin;
    stdin.type = VDISK_STREAM;
    strcpy(stdin.drive_name, "stdin");
    stdin.flag = 1;
    stdin.sector_size = 1;
    stdin.size = 1;
    stdin.write = (void*)empty;
    stdin.read = stdin_read;
    regist_vdisk(stdin);
}

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
    if (indx >= 26) {
        return false;
    }
    if (vdisk_ctl[indx].flag > 0) {
        return true;
    } else {
        return false;
    }
}

uint32_t disk_size(int drive) {
    uint8_t drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1;
        return vdisk_ctl[indx].size;
    } else {
        return 0;
    }
}

int rw_vdisk(int drive, uint32_t lba, uint8_t *buffer, uint32_t number, int read) {
    int indx = drive;
    if (indx >= 26) {
        return 0;
    }
    if (vdisk_ctl[indx].flag > 0) {
        if (read) {
            vdisk_ctl[indx].read(drive, buffer, number, lba);
        } else {
            vdisk_ctl[indx].write(drive, buffer, number, lba);
        }
        return 1;
    } else {
        return 0;
    }
}

void vdisk_read(uint32_t lba, uint32_t number, void *buffer, int drive) {
    if (have_vdisk(drive)) {
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            rw_vdisk(drive, lba + i, (uint8_t *) ((uint64_t) buffer + i * vdisk_ctl[drive].sector_size), sectors, 1);
        }
    }
}

void vdisk_write(uint32_t lba, uint32_t number, const void *buffer, int drive) {
    if (have_vdisk(drive)) {
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            rw_vdisk(drive, lba + i, (uint8_t *) ((uint64_t) buffer + i * vdisk_ctl[drive].sector_size), sectors,
                     0);
        }
    }
}

int vdisk_init() {
    for (size_t i = 0; i < 26; i++) {
        vdisk_ctl[i].flag = 0; // 设置为未使用
    }
    return 0;
}
