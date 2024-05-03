#ifndef CRASHPOWEROS_FAT_H
#define CRASHPOWEROS_FAT_H

#include "../include/common.h"

struct FAT_CACHE {
    unsigned int ADR_DISKIMG;
    struct FAT_FILEINFO *root_directory;
    struct LIST *directory_list;
    struct LIST *directory_max_list;
    struct LIST *directory_clustno_list;
    int *fat;
    int FatMaxTerms;
    unsigned int ClustnoBytes;
    unsigned short RootMaxFiles;
    unsigned int RootDictAddress;
    unsigned int FileDataAddress;
    unsigned int imgTotalSize;
    unsigned short SectorBytes;
    unsigned int Fat1Address, Fat2Address;
    unsigned char *FatClustnoFlags;
    int type;
};
typedef struct {
    struct FAT_CACHE dm;
    struct FAT_FILEINFO *dir;
} fat_cache;

#define get_dm(vfs) ((fat_cache *)(vfs->cache))->dm
#define get_now_dir(vfs) ((fat_cache *)(vfs->cache))->dir
#define get_clustno(high, low) (high << 16) | (low & 0xffff)
#define clustno_end(type) 0xfffffff & ((((1 << (type - 1)) - 1) << 1) + 1)

struct FAT_FILEINFO {
    unsigned char name[8], ext[3], type;
    char reserve;
    unsigned char create_time_tenth;
    unsigned short create_time, create_date, access_date, clustno_high;
    unsigned short update_time, update_date, clustno_low;
    unsigned int size;
};

typedef struct FILE {
    unsigned int mode;
    unsigned int fileSize;
    unsigned char *buffer;
    unsigned int bufferSize;
    unsigned int p;
    char *name;
} FILE;

typedef enum { FLE, DIR, RDO, HID, SYS } ftype;
typedef struct {
    char name[255];
    ftype type;
    unsigned int size;
    unsigned short year, month, day;
    unsigned short hour, minute;
} vfs_file;

typedef struct vfs_t {
    struct List *path;
    void *cache;
    char FSName[255];
    int disk_number;
    uint8_t drive; // 大写（必须）
    vfs_file *(*FileInfo)(struct vfs_t *vfs, char *filename);
    struct List *(*ListFile)(struct vfs_t *vfs, char *dictpath);
    bool (*ReadFile)(struct vfs_t *vfs, char *path, char *buffer);
    bool (*WriteFile)(struct vfs_t *vfs, char *path, char *buffer, int size);
    bool (*DelFile)(struct vfs_t *vfs, char *path);
    bool (*DelDict)(struct vfs_t *vfs, char *path);
    bool (*CreateFile)(struct vfs_t *vfs, char *filename);
    bool (*CreateDict)(struct vfs_t *vfs, char *filename);
    bool (*RenameFile)(struct vfs_t *vfs, char *filename, char *filename_of_new);
    bool (*Attrib)(struct vfs_t *vfs, char *filename, ftype type);
    bool (*Format)(uint8_t disk_number);
    void (*InitFs)(struct vfs_t *vfs, uint8_t disk_number);
    void (*DeleteFs)(struct vfs_t *vfs);
    bool (*Check)(uint8_t disk_number);
    bool (*cd)(struct vfs_t *vfs, char *dictName);
    int (*FileSize)(struct vfs_t *vfs, char *filename);
    void (*CopyCache)(struct vfs_t *dest, struct vfs_t *src);
    int flag;
} vfs_t;
/*
void read_fat(unsigned char *img, int *fat, unsigned char *ff, int max,int type);
int get_directory_max(struct FAT_FILEINFO *directory, vfs_t *vfs);
void file_loadfile(int clustno, int size, char *buf, int *fat, vfs_t *vfs);
void file_savefat(int *fat, int clustno, int length, vfs_t *vfs);
 */

#endif
