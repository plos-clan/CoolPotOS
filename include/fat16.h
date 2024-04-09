#ifndef CPOS_FAT16_H
#define CPOS_FAT16_H

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

void read_root_dir_sector1(struct File *root_entries);
void save_root_dir_sector1(struct File *root_entries);
void read_one_cluster(unsigned short clustno, unsigned int memory_addrress);
int read_root_dir(struct File *file_infos);
void get_fat1(unsigned short *fat1);
void save_fat1(unsigned short *fat1);
void get_file_all_clustnos(unsigned short first_clustno, unsigned short *clustnos);
void get_file_all_clustnos(unsigned short first_clustno, unsigned short *clustnos);
void read_file(struct File *file, void *file_addr);
void check_name_or_ext(char *str, int len);
void check_name_and_ext(char *name, char *ext);
void analyse_fullname(char *fullname, char *name, char *ext);
int find_file(char *name, char *ext, struct File *const file);
struct File *create_file(char *fullname);
struct File *create_dir(char *fullname);
struct File *open_file(char *fullname);
void alter_file_name(struct File *file, char *new_fullname);
void alter_dir_entry(struct File *file);
void save_file(struct File *file, char *content);
void delete_file(struct File *file);

#endif