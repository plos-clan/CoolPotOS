#ifndef CPOS_DISK_H
#define CPOS_DISK_H

//[该方法IDE硬盘正常，但SATA硬盘会表现为一直忙，无限循环卡在这里]
void wait_disk_ready();

//向硬盘控制器写入起始扇区地址及要读写的扇区数
void select_sector(int lba);

/**读一个扇区（经测试，一次读多个扇区会有各种问题，需要分开一个扇区一个扇区的读。）
 * lba LBA28扇区号
 * memory_addrress 将数据写入的内存地址
 */
void read_disk_one_sector(int lba, unsigned int memory_addrress);
void write_disk_one_sertor(int lba, unsigned int memory_addrress);

/**读取硬盘函数（IDE主通道主盘）
 * lba 起始LBA28扇区号
 * sector_count 读入的扇区数
 * memory_addrress 将数据写入的内存地址
 */
void read_disk(int lba, int sector_count, unsigned int memory_addrress);

/**写入硬盘函数（IDE主通道主盘）
 * lba 起始LBA28扇区号
 * sector_count 写入的扇区数
 * memory_addrress 将数据写入的内存地址
 */
void write_disk(int lba, int sector_count, unsigned int memory_addrress);

#endif
