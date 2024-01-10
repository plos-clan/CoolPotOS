#ifndef CPOS_FATFS_H
#define CPOS_FATFS_H

#include <stddef.h>

#define SECTOR_SIZE 512
#define FAT1_SECTORS 32                                     //FAT1占用扇区数
#define ROOT_DIR_SECTORS 32                                 //根目录占用扇区数
#define SECTOR_NUM_OF_FAT1_START 1                          //FAT1起始扇区号
#define SECTOR_NUM_OF_ROOT_DIR_START 33                     //根目录起始扇区号
#define SECTOR_NUM_OF_DATA_START 65                         //数据区起始扇区号，对应簇号为2。
#define SECTOR_CLUSTER_BALANCE SECTOR_NUM_OF_DATA_START - 2 //簇号加上该值正好对应扇区号。
#define MAX_FILE_NUM 16                                     //最大文件数

// FAT16目录项结构(32B);
struct File {
    // 文件名 如果第一个字节为0xe5，代表这个文件已经被删除；如果第一个字节为0x00，代表这一段不包含任何文件名信息。
    unsigned char name[8];
    unsigned char ext[3]; // 扩展名
    // 属性：bit0只读文件，bit1隐藏文件，bit2系统文件，bit3非文件信息(比如磁盘名称)，bit4目录，bit5文件。
    unsigned char type; // 0x20 文件 | 0x10 目录
    unsigned char reserve[10]; // 保留
    unsigned short time;       // 最后一次写入时间
    unsigned short date;       // 最后一次写入日期
    unsigned short clustno;    // 起始簇号
    unsigned int size;         // 文件大小
};

//读根目录的第1个扇区。root_entries为存放数据的内存。
void read_root_dir_sector1(struct File *root_entries);
//写根目录的第1个扇区。
void save_root_dir_sector1(struct File *root_entries);
//读取1个簇到指定的内存地址
void read_one_cluster(unsigned short clustno, unsigned int memory_addrress);
/**读根目录有效文件项
 * file_info 将找到的文件项写入该数组中，要求该数组项数量为16。
 * 返回找到的文件数量
 */
int read_root_dir(struct File *file_infos);
//返回fat1表的全部数据。
void get_fat1(unsigned short *fat1);
void save_fat1(unsigned short *fat1);
//获取文件的所有簇号，存放到clustnos中。
void get_file_all_clustnos(unsigned short first_clustno, unsigned short *clustnos);
//获取文件的所有簇号，存放到clustnos中。
void get_file_all_clustnos(unsigned short first_clustno, unsigned short *clustnos);
//读文件。file_addr是将文件读取到的内存地址。
void read_file(struct File *file, void *file_addr);

/**检查文件名或扩展名，如果有不符合规则的地方则自动修正。
 * str 文件名或扩展名
 * len str的长度
 */
void check_name_or_ext(char *str, int len);
//检查文件名和扩展名，如果有不符合规则的地方则自动修正。
void check_name_and_ext(char *name, char *ext);
/**解析文件全名
 * fullname 文件全名
 * name 存放解析好的文件名
 * ext 存放解析好的扩展名
 */
void analyse_fullname(char *fullname, char *name, char *ext);
/**查找文件。找到返回1，没找到返回0。
 * name 文件名
 * ext 文件扩展名
 * file 如果file不为0，在找到文件时将相关数据赋给该指针指向的结构体。
 */
int find_file(char *name, char *ext, struct File *const file);
//创建文件，正常返回文件指针，如果文件已存在返回0。
struct File *create_file(char *fullname);
struct File *create_dir(char *fullname);
//打开一个文件
struct File *open_file(char *fullname);
//修改文件名
void alter_file_name(struct File *file, char *new_fullname);
//修改文件夹目录项（修改文件属性）
void alter_dir_entry(struct File *file);
//保存文件
void save_file(struct File *file, char *content);
//删除文件
void delete_file(struct File *file);

#endif
