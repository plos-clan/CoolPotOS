#ifndef CPOS_DISK_H
#define CPOS_DISK_H

void wait_disk_ready(); //不适用于SATA硬盘
void select_sector(int lba);
void read_disk_one_sector(int lba, unsigned int memory_addrress);
void write_disk_one_sertor(int lba, unsigned int memory_addrress);
void read_disk(int lba, int sector_count, unsigned int memory_addrress);
void write_disk(int lba, int sector_count, unsigned int memory_addrress);

#endif