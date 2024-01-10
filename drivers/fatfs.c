#include "../include/fatfs.h"
#include "../include/kheap.h"
#include "../include/disk.h"
#include "../include/memory.h"
#include "../include/console.h"

void read_root_dir_sector1(struct File *root_entries) {
    read_disk(SECTOR_NUM_OF_ROOT_DIR_START, 1, (unsigned int) root_entries);
}

void save_root_dir_sector1(struct File *root_entries) {
    write_disk(SECTOR_NUM_OF_ROOT_DIR_START, 1, (unsigned int) root_entries);
}

void read_one_cluster(unsigned short clustno, unsigned int memory_addrress) {
    read_disk(clustno + SECTOR_CLUSTER_BALANCE, 1, memory_addrress);
}

int read_root_dir(struct File *file_infos) {
    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    int n = 0; //记录文件数
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (root_entries[i].name[0] == 0) {
            break;
        }
        if (root_entries[i].name[0] != 0xe5) {
            file_infos[n] = root_entries[i];
            n++;
        }
    }
    kfree(root_entries);
    return n;
}

void get_fat1(unsigned short *fat1) {
    read_disk(SECTOR_NUM_OF_FAT1_START, FAT1_SECTORS, (unsigned int) fat1); //将FAT1全部读取到内存中。
}

void save_fat1(unsigned short *fat1) {
    write_disk(SECTOR_NUM_OF_FAT1_START, FAT1_SECTORS, (unsigned int) fat1); //将FAT1全部写回到内存中。
}

void get_file_all_clustnos(unsigned short first_clustno, unsigned short *clustnos) {
    unsigned short *fat1 = kmalloc(FAT1_SECTORS * SECTOR_SIZE);
    get_fat1(fat1);
    *clustnos = first_clustno;
    while (1) {
        unsigned short clustno = *(fat1 + *clustnos);
        clustnos++;
        *clustnos = clustno;
        if (clustno >= 0xfff8) //大于等于0xfff8表示文件的最后一个簇
        {
            break;
        }
    }
    kfree(fat1);
}

void read_file(struct File *file, void *file_addr) {
    if (file->size == 0) {
        return;
    }
    int cluster_count = (file->size + 511) / 512; //簇的数量
    //申请用来存放簇号的数组空间，每个簇号占用2个字节。
    unsigned short *clustnos = kmalloc(cluster_count * 2);
    get_file_all_clustnos(file->clustno, clustnos);
    for (int i = 0; i < cluster_count; i++) {
        read_one_cluster(clustnos[i], (unsigned int) file_addr);
        file_addr += 512;
    }
    kfree(clustnos);
}

void check_name_or_ext(char *str, int len) {
    for (int i = 0; i < len; i++) {
        if (str[i] == 0) {
            str[i] = ' ';
        } else if ('a' <= str[i] && str[i] <= 'z') {
            str[i] -= 0x20;
        }
    }
}

void check_name_and_ext(char *name, char *ext) {
    check_name_or_ext(name, 8);
    check_name_or_ext(ext, 3);
}

void analyse_fullname(char *fullname, char *name, char *ext) {
    int ext_dot_index = -1;
    for (int i = 11; i >= 0; i--) {
        if (fullname[i] == '.') {
            ext_dot_index = i;
        }
    }
    if (ext_dot_index == -1) //没有后缀名的情况
    {
        memcpy(name, fullname, 8);
        memset(ext, ' ', 3);
    } else if (ext_dot_index == 0) //没有文件名的情况
    {
        memset(name, ' ', 8);
        memcpy(ext, fullname + 1, 3);
    } else {
        memcpy(name, fullname, ext_dot_index);
        memcpy(ext, fullname + ext_dot_index + 1, 3);
    }
    check_name_and_ext(name, ext);
}

int find_file(char *name, char *ext, struct File *const file) {
    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    int isfind = 0;
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (memcmp(root_entries[i].name, name, 8) == 0 && memcmp(root_entries[i].ext, ext, 3) == 0) {
            if (file != 0) {
                *file = root_entries[i];
            }
            isfind = 1;
        }
    }
    kfree(root_entries);
    return isfind;
}
/*
//如果有打开的文件则关闭
void close_file(void)
{
    struct Task *task = task_now();
    if (task->opened_file != 0)
    {
        kfree(task->opened_file);
        task->opened_file = 0;
    }
}
 */

/*
void set_opened_file(struct File *file)
{
    if (file == 0)
        return;
    close_file();
    struct Task *task = task_now();
    task->opened_file = file;
}
 */

struct File *create_dir(char *fullname){
    char name[8] = {0};
    char ext[3] = {0};
    analyse_fullname(fullname, name, ext);
    int isfind = find_file(name, ext, 0);
    if (isfind) {
        return 0; //文件已存在。
    }

    struct File *file = (struct File *) kmalloc(32);
    memcpy(file->name, name, 8);
    memcpy(file->ext, "   ", 3);
    file->type = 0x10;
    file->time = 0;
    file->date = 0;
    file->clustno = 0;
    file->size = 0;

    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (root_entries[i].name[0] == 0 || root_entries[i].name[0] == 0xe5) { //找一个空闲的文件项存放。
            root_entries[i] = *file;
            break;
        }
    }
    save_root_dir_sector1(root_entries);
    kfree(root_entries);
    return file;
}

struct File *create_file(char *fullname) {
    char name[8] = {0};
    char ext[3] = {0};
    analyse_fullname(fullname, name, ext);
    int isfind = find_file(name, ext, 0);
    if (isfind) {
        return 0; //文件已存在。
    }

    struct File *file = (struct File *) kmalloc(32);
    memcpy(file->name, name, 8);
    memcpy(file->ext, ext, 3);
    file->type = 0x20;
    file->time = 0;
    file->date = 0;
    file->clustno = 0;
    file->size = 0;

    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (root_entries[i].name[0] == 0 || root_entries[i].name[0] == 0xe5) { //找一个空闲的文件项存放。
            root_entries[i] = *file;
            break;
        }
    }
    save_root_dir_sector1(root_entries);



    kfree(root_entries);
    //set_opened_file(file);
    return file;
}

struct File *open_file(char *fullname) {
    char name[8] = {0};
    char ext[3] = {0};
    analyse_fullname(fullname, name, ext);
    struct File *file = (struct File *) kmalloc(32);
    int isfind = find_file(name, ext, file);
    if (!isfind) {
        kfree(file);
        file = 0;
    } else {
        //set_opened_file(file);
    }
    return file;
}

void alter_file_name(struct File *file, char *new_fullname) {
    char new_name[8] = {0};
    char new_ext[3] = {0};
    analyse_fullname(new_fullname, new_name, new_ext);
    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (memcmp(root_entries[i].name, file->name, 8) == 0 && memcmp(root_entries[i].ext, file->ext, 3) == 0) {
            memcpy(root_entries[i].name, new_name, 8);
            memcpy(root_entries[i].ext, new_ext, 3);
        }
    }
    save_root_dir_sector1(root_entries);
    kfree(root_entries);
}

void alter_dir_entry(struct File *file) {
    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (memcmp(root_entries[i].name, file->name, 8) == 0 && memcmp(root_entries[i].ext, file->ext, 3) == 0) {
            root_entries[i] = *file;
        }
    }
    save_root_dir_sector1(root_entries);
    kfree(root_entries);
}

void save_file(struct File *file, char *content) {
    unsigned int file_size = strlen(content); //内容大小
    if (file_size == 0 && file->size == 0) {
        return;
    }
    file->size = file_size;
    int sector_num = (file_size + 511) / 512; //将要占用的扇区数
    unsigned short *fat1 = kmalloc(FAT1_SECTORS * SECTOR_SIZE);
    get_fat1(fat1);
    unsigned short clustno = file->clustno;
    unsigned short next_clustno;
    if (file->clustno == 0) //第一次写入
    {
        clustno = 2; //可用簇号从2开始
        while (1) {
            if (*(fat1 + clustno) == 0) //空闲簇号
            {
                file->clustno = clustno; //分配起始簇号
                break;
            } else {
                clustno++;
            }
        }
    }
    int i = 0;
    while (1) {
        write_disk(SECTOR_CLUSTER_BALANCE + clustno, 1, (unsigned int) content);
        if (i == sector_num - 1) { //已写完最后一扇区。
            break;
        }
        i++;
        content += 512;
        next_clustno = *(fat1 + clustno);
        if (next_clustno == 0 || next_clustno >= 0xfff8) { //寻找下一个可用簇号
            next_clustno = clustno + 1;
            while (1) {
                if (*(fat1 + next_clustno) == 0) {
                    *(fat1 + clustno) = next_clustno;
                    break;
                } else {
                    next_clustno++;
                }
            }
        }
        clustno = next_clustno;
    }
    next_clustno = *(fat1 + clustno);
    *(fat1 + clustno) = 0xffff; //0xfff8~0xffff表示文件结束，没有下一个簇了。
    //如果本次写入的内容比上次的少，上次用过的簇号本次用不完，以下将剩下的簇号清零处理。
    if (1 < next_clustno && next_clustno < 0xfff0) {
        while (1) {
            clustno = next_clustno;
            next_clustno = *(fat1 + clustno);
            *(fat1 + clustno) = 0;
            if (next_clustno >= 0xfff8) {
                break;
            }
        }
    }
    alter_dir_entry(file);
    save_fat1(fat1);
    kfree(fat1); //释放内存
}

void delete_file(struct File *file) {
    //1.标记目录项为已删除。
    struct File *root_entries = kmalloc(SECTOR_SIZE);
    read_root_dir_sector1(root_entries);
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        if (memcmp(root_entries[i].name, file->name, 8) == 0 && memcmp(root_entries[i].ext, file->ext, 3) == 0) {
            root_entries[i].name[0] = 0xe5;
            break;
        }
    }
    save_root_dir_sector1(root_entries);
    kfree(root_entries);
    if (file->clustno == 0) {
        return;
    }
    unsigned short *fat1 = kmalloc(FAT1_SECTORS * SECTOR_SIZE);
    get_fat1(fat1);
    unsigned short clustno = file->clustno;
    unsigned short next_clustno = 0;
    while (1) {
        next_clustno = *(fat1 + clustno);
        *(fat1 + clustno) = 0;
        if (next_clustno >= 0xfff8) {
            break;
        }
        clustno = next_clustno;
    }
    save_fat1(fat1);
    kfree(fat1);
}