#include "../include/disk.h"
#include "../include/io.h"

void wait_disk_ready() {
    while (1) {
        int data = io_in8(0x1f7);
        if ((data & 0x88) == 0x08) //第3位为1表示硬盘控制器已准备好数据传输，第7位为1表示硬盘忙。
        {
            return;
        }
    }
}

void select_sector(int lba) {
    io_out8(0x1f2, 1);
    io_out8(0x1f3, lba);
    io_out8(0x1f4, lba >> 8);
    io_out8(0x1f5, lba >> 16);
    io_out8(0x1f6, (((lba >> 24) & 0x0f) | 0xe0));
}

void read_disk_one_sector(int lba, unsigned int memory_addrress) {
    while (io_in8(0x1f7) & 0x80);
    select_sector(lba);
    io_out8(0x1f7, 0x20);
    wait_disk_ready();
    for (int i = 0; i < 256; i++) {
        short data = (short) io_in16(0x1f0);
        *((short *) memory_addrress) = data;
        memory_addrress += 2;
    }
}

void write_disk_one_sertor(int lba, unsigned int memory_addrress) {
    while (io_in8(0x1f7) & 0x80);
    select_sector(lba);
    io_out8(0x1f7, 0x30);
    wait_disk_ready();
    for (int i = 0; i < 256; i++) {
        short data = *((short *) memory_addrress);
        io_out16(0x1f0, data);
        memory_addrress += 2;
    }
}

void read_disk(int lba, int sector_count, unsigned int memory_addrress) {
    for (int i = 0; i < sector_count; i++) {
        read_disk_one_sector(lba, memory_addrress);
        lba++;
        memory_addrress += 512;
    }
}

void write_disk(int lba, int sector_count, unsigned int memory_addrress) {
    for (int i = 0; i < sector_count; i++) {
        write_disk_one_sertor(lba, memory_addrress);
        lba++;
        memory_addrress += 512;
    }
}

