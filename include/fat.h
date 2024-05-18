#ifndef CRASHPOWERDOS_FAT_H
#define CRASHPOWERDOS_FAT_H

#define BS_FileSysType 54
#define BPB_Fat32ExtByts 28
#define BS_jmpBoot 0
#define BS_OEMName 3
#define BPB_BytsPerSec 11
#define BPB_SecPerClus 13
#define BPB_RsvdSecCnt 14
#define BPB_NumFATs 16
#define BPB_RootEntCnt 17
#define BPB_TotSec16 19
#define BPB_Media 21
#define BPB_FATSz16 22
#define BPB_SecPerTrk 24
#define BPB_NumHeads 26
#define BPB_HiddSec 28
#define BPB_TotSec32 32
#define BPB_FATSz32 36
#define BPB_ExtFlags 40
#define BPB_FSVer 42
#define BPB_RootClus 44
#define BPB_FSInfo 48
#define BPB_BkBootSec 50
#define BPB_Reserved 52
#define BPB_Fat32ExtByts 28
#define BS_DrvNum 36
#define BS_Reserved1 37
#define BS_BootSig 38
#define BS_VolD 39
#define BS_VolLab 43
#define BS_FileSysType 54
#define EOF -1
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#include "list.h"
#include "vfs.h"

struct FAT_FILEINFO {
    unsigned char name[8], ext[3], type;
    char reserve;
    unsigned char create_time_tenth;
    unsigned short create_time, create_date, access_date, clustno_high;
    unsigned short update_time, update_date, clustno_low;
    unsigned int size;
};

struct FAT_CACHE {
    unsigned int ADR_DISKIMG;
    struct FAT_FILEINFO *root_directory;
    struct List *directory_list;
    struct List *directory_clustno_list;
    struct List *directory_max_list;
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

void read_fat(unsigned char *img, int *fat, unsigned char *ff, int max,
              int type);
int get_directory_max(struct FAT_FILEINFO *directory, vfs_t *vfs);
int changedict(char *dictname, vfs_t *vfs);
struct FAT_FILEINFO *dict_search(char *name, struct FAT_FILEINFO *finfo,
                                 int max);
void file_savefat(int *fat, int clustno, int length, vfs_t *vfs);
void file_saveinfo(struct FAT_FILEINFO *directory, vfs_t *vfs);
void file_savefile(int clustno, int size, char *buf, int *fat,
                   unsigned char *ff, vfs_t *vfs);
void file_loadfile(int clustno, int size, char *buf, int *fat, vfs_t *vfs);
int deldir(char *path, vfs_t *vfs);
int del(char *cmdline, vfs_t *vfs);
int attrib(char *filename, ftype type, struct vfs_t *vfs);
int rename(char *src_name, char *dst_name, vfs_t *vfs);
int mkdir(char *dictname, int last_clust, vfs_t *vfs);
int mkfile(char *name, vfs_t *vfs);
void fat_InitFS(struct vfs_t *vfs, uint8_t disk_number);
bool Fat_Check(uint8_t disk_number);
void Fat_CopyCache(struct vfs_t *dest, struct vfs_t *src);
List *Fat_ListFile(struct vfs_t *vfs, char *dictpath);
bool Fat_WriteFile(struct vfs_t *vfs, char *path, char *buffer, int size);
bool Fat_ReadFile(struct vfs_t *vfs, char *path, char *buffer);
bool Fat_RenameFile(struct vfs_t *vfs, char *filename, char *filename_of_new);
bool Fat_CreateFile(struct vfs_t *vfs, char *filename);
void Fat_DeleteFs(struct vfs_t *vfs);
bool Fat_DelFile(struct vfs_t *vfs, char *path);
bool Fat_DelDict(struct vfs_t *vfs, char *path);
bool Fat_Format(uint8_t disk_number);
vfs_file *Fat_FileInfo(struct vfs_t *vfs, char *filename);
bool Fat_Attrib(struct vfs_t *vfs, char *filename, ftype type);
bool Fat_CreateDict(struct vfs_t *vfs, char *filename);
bool Fat_cd(struct vfs_t *vfs, char *dictName);
struct FAT_FILEINFO *Get_dictaddr(char *path1, vfs_t *vfs);
void Register_fat_fileSys();

#endif