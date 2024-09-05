#ifndef CRASHPOWEROS_VFS_H
#define CRASHPOWEROS_VFS_H

#define toupper(c) ((c) >= 'a' && (c) <= 'z' ? c - 32 : c)

#include "common.h"
#include "list.h"

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

bool vfs_change_path(char *dictName);
void vfs_getPath(char *buffer);
void vfs_getPath_no_drive(char *buffer);
bool vfs_mount_disk(uint8_t disk_number, uint8_t drive);
bool vfs_unmount_disk(uint8_t drive);
bool vfs_readfile(char *path, char *buffer);
bool vfs_writefile(char *path, char *buffer, int size);
uint32_t vfs_filesize(char *filename);
List *vfs_listfile(char *dictpath);
bool vfs_delfile(char *filename);
bool vfs_deldir(char *dictname);
bool vfs_createfile(char *filename);
bool vfs_createdict(char *filename);
bool vfs_renamefile(char *filename, char *filename_of_new);
bool vfs_attrib(char *filename, ftype type);
bool vfs_format(uint8_t disk_number, char *FSName);
vfs_file *vfs_fileinfo(char *filename);

#include "task.h"

bool vfs_change_disk(struct task_struct *task,uint8_t drive);
bool vfs_register_fs(vfs_t vfs);
void init_vfs();
vfs_file *get_cur_file(char* filename);
void vfs_copy(struct task_struct *task,vfs_t* src);

#endif
