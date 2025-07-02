#include "krlibc.h"
#include "timer.h"
#include "vdisk.h"

static size_t urandom_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    for (size_t i = 0; i < number; i++) {
        uint64_t speed = nano_time() / 32768 + 12345;
        buffer[i]      = (uint8_t)((speed >> (i % 8)) & 0xFF);
    }
    return number;
}

void setup_urandom() {
    vdisk urandom;
    urandom.type        = VDISK_STREAM;
    urandom.size        = 1;
    urandom.map         = (void *)empty;
    urandom.read        = urandom_read;
    urandom.write       = (void *)empty;
    urandom.ioctl       = (void *)empty;
    urandom.poll        = (void *)empty;
    urandom.flag        = 1;
    urandom.sector_size = 1;
    strcpy(urandom.drive_name, "urandom");
    regist_vdisk(urandom);
}
