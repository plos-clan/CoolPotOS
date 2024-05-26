/*
 * PlantsOS FileSystem Abstract Interface
 * Copyright by min0911
 */
#include "../include/printf.h"
#include "../include/vfs.h"
#include "../include/common.h"
#include "../include/memory.h"

vfs_t vfsstl[26];
vfs_t vfsMount_Stl[26];
vfs_t *vfs_now;

static vfs_t *drive2fs(uint8_t drive) {
    for (int i = 0; i < 26; i++) {
        if (vfsMount_Stl[i].drive == toupper(drive) && vfsMount_Stl[i].flag == 1) {
            return &vfsMount_Stl[i];
        }
    }
    return NULL;
}

static vfs_t *ParsePath(char *result) {
    vfs_t *vfs_result = vfs_now;
    if (result[1] == ':') {
        if (!(vfs_result = drive2fs(result[0]))) {
            printf("Mount Drive is not found!\n");
            printf("Parse Error.\n");
            return NULL;
        }
        if (result) {
            delete_char(result, 0);
            delete_char(result, 0);
        }
    }
    if (result) {
        for (int i = 0; i < strlen(result); i++) {
            if (result[i] == '\\') {
                result[i] = '/';
            }
        }
    }
    return vfs_result;
}

static vfs_t *findSeat(vfs_t *vstl) {
    for (int i = 0; i < 26; i++) {
        if (vstl[i].flag == 0) {
            return &vstl[i];
        }
    }
    return NULL;
}

static vfs_t *check_disk_fs(uint8_t disk_number) {
    for (int i = 0; i < 26; i++) {
        if (vfsstl[i].flag == 1) {
            if (vfsstl[i].Check(disk_number)) {
                return &vfsstl[i];
            }
        }
    }

    return NULL;
}

static void insert_str1(char *str, char *insert_str1, int pos) {
    for (int i = 0; i < strlen(insert_str1); i++) {
        insert_char(str, pos + i, insert_str1[i]);
    }
}

bool vfs_mount_disk(uint8_t disk_number, uint8_t drive) {
    printf("Mount DISK ---- %02x\n", disk_number);
    for (int i = 0; i < 26; i++) {
        if (vfsMount_Stl[i].flag == 1 &&
            (vfsMount_Stl[i].drive == drive ||
             vfsMount_Stl[i].disk_number == disk_number)) {
            return false;
        }
    }

    vfs_t *seat = findSeat(vfsMount_Stl);
    if (!seat) {
        printf("can not find a seat of vfsMount_Stl(it's full)\n");
        return false;
    }
    vfs_t *fs = check_disk_fs(disk_number);
    if (!fs) {
        printf("[FileSystem]: Unknown file system.\n");
        return false;
    }
    *seat = *fs;
    seat->InitFs(seat, disk_number);
    seat->drive = drive;
    seat->disk_number = disk_number;
    seat->flag = 1;
    return true;
}

bool vfs_unmount_disk(uint8_t drive) {
    printf("Unmount disk ---- %c\n", drive);
    for (int i = 0; i < 26; i++) {
        if (vfsMount_Stl[i].drive == drive && vfsMount_Stl[i].flag == 1) {
            vfsMount_Stl[i].DeleteFs(&vfsMount_Stl[i]);
            vfsMount_Stl[i].flag = 0;
            return true;
        }
    }
    printf("Not found the drive.\n");
    return false;
}

bool vfs_readfile(char *path, char *buffer) {
    char *new_path = kmalloc(strlen(path) + 1);
    strcpy(new_path, path);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->ReadFile(vfs, new_path, buffer);
    kfree(new_path);
    return result;
}

bool vfs_writefile(char *path, char *buffer, int size) {
    char *new_path = kmalloc(strlen(path) + 1);
    strcpy(new_path, path);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->WriteFile(vfs, new_path, buffer, size);
    kfree(new_path);
    return result;
}

uint32_t vfs_filesize(char *filename) {
    char *new_path = kmalloc(strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return -1;
    }
    int result = vfs->FileSize(vfs, new_path); // 没找到文件统一返回-1
    kfree(new_path);
    return result;
}

List *vfs_listfile(char *dictpath) { // dictpath == "" 则表示当前路径
    if (strcmp(dictpath, "") == 0) {
        return vfs_now->ListFile(vfs_now, dictpath);
    } else {
        char *new_path = kmalloc(strlen(dictpath) + 1);
        strcpy(new_path, dictpath);
        vfs_t *vfs = ParsePath(new_path);
        if (vfs == NULL) {
            kfree(new_path);
            return NULL;
        }
        List *result = vfs->ListFile(vfs, new_path);
        kfree(new_path);
        return result;
    }
}

bool vfs_delfile(char *filename) {
    char *new_path = kmalloc(strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->DelFile(vfs, new_path);
    kfree(new_path);
    return result;
}

bool vfs_change_path(char *dictName) {
    char *buf = kmalloc(strlen(dictName) + 1);
    char *r = buf;
    memcpy(buf, dictName, strlen(dictName) + 1);
    int i = 0;
    if (buf[i] == '/' || buf[i] == '\\') {
        if (!vfs_now->cd(vfs_now, "/")) {
            kfree(r);
            return false;
        }
        i++;
        buf++;
    }

    for (;; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            buf[i] = 0;
            if (!vfs_now->cd(vfs_now, buf)) {
                kfree(r);
                return false;
            }
            buf += strlen(buf) + 1;
        }
        if (buf[i] == 0) {
            if (!vfs_now->cd(vfs_now, buf)) {
                kfree(r);
                return false;
            }
            break;
        }
    }
    kfree(r);
    return true;
}

bool vfs_deldir(char *dictname) {
    char *new_path = kmalloc(strlen(dictname) + 1);
    strcpy(new_path, dictname);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->DelDict(vfs, new_path);
    kfree(new_path);
    return result;
}

bool vfs_createfile(char *filename) {
    char *new_path = kmalloc(strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->CreateFile(vfs, new_path);
    kfree(new_path);
    return result;
}

bool vfs_createdict(char *filename) {
    char *new_path = kmalloc(strlen(filename) + 1);
    memclean(new_path, strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->CreateDict(vfs, new_path);
    kfree(new_path);
    return result;
}

bool vfs_renamefile(char *filename, char *filename_of_new) {
    char *new_path = kmalloc(strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->RenameFile(vfs, new_path, filename_of_new);
    kfree(new_path);
    return result;
}

bool vfs_attrib(char *filename, ftype type) {
    char *new_path = kmalloc(strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }
    int result = vfs->Attrib(vfs, new_path, type);
    kfree(new_path);
    return result;
}

bool vfs_format(uint8_t disk_number, char *FSName) {
    for (int i = 0; i < 255; i++) {
        if (strcmp(vfsstl[i].FSName, FSName) == 0 && vfsstl[i].flag == 1) {
            return vfsstl[i].Format(disk_number);
        }
    }
    return false;
}

vfs_file *vfs_fileinfo(char *filename) {
    char *new_path = kmalloc(strlen(filename) + 1);
    memclean(new_path,strlen(filename) + 1);
    strcpy(new_path, filename);
    vfs_t *vfs = ParsePath(new_path);
    if (vfs == NULL) {
        kfree(new_path);
        return false;
    }



    vfs_file *result = vfs->FileInfo(vfs, new_path);
    kfree(new_path);
    return result;
}

vfs_file *get_cur_file(char* filename){
    vfs_file *file = NULL;
    List *ls = vfs_listfile("");
    strtoupper(filename);
    for (int i = 1; FindForCount(i, ls) != NULL; i++) {
        vfs_file *d = (vfs_file *) FindForCount(i, ls)->val;
        if(strcmp(d->name, filename) == 0){
            file = d;
            break;
        }
        kfree(d);
    }
    DeleteList(ls);
    kfree(ls);



    return file;
}

bool vfs_change_disk(uint8_t drive) {
    if (vfs_now != NULL) {
        while (FindForCount(1, vfs_now->path) != NULL) {
            kfree(FindForCount(vfs_now->path->ctl->all, vfs_now->path)->val);
            DeleteVal(vfs_now->path->ctl->all, vfs_now->path);
        }
        kfree(vfs_now->cache);
        DeleteList(vfs_now->path);
        kfree(vfs_now);
    }

    vfs_t *f;
    if (!(f = drive2fs(drive))) {
        return false; // 没有mount
    }

    vfs_now = kmalloc(sizeof(vfs_t));
    memcpy(vfs_now, f, sizeof(vfs_t));
    f->CopyCache(vfs_now, f);
    vfs_now->path = NewList();
    vfs_now->cd(vfs_now, "/");
    return true;
}

void vfs_getPath(char *buffer) {
    char *path;
    List *l;
    buffer[0] = 0;
    insert_char(buffer, 0, vfs_now->drive);
    insert_char(buffer, 1, ':');
    insert_char(buffer, 2, '\\');
    int pos = strlen(buffer);
    for (int i = 1; FindForCount(i, vfs_now->path) != NULL; i++) {
        l = FindForCount(i, vfs_now->path);
        path = (char *)l->val;
        insert_str(buffer, path, pos);
        pos += strlen(path);
        insert_char(buffer, pos, '\\');
        pos++;
    }
    delete_char(buffer, pos - 1);
}

void vfs_getPath_no_drive(char *buffer) {
    char *path;
    List *l;
    buffer[0] = 0;
    int pos = strlen(buffer);
    int i;
    for (i = 1; FindForCount(i, vfs_now->path) != NULL; i++) {
        l = FindForCount(i, vfs_now->path);
        path = (char *)l->val;
        insert_char(buffer, pos, '/');
        pos++;
        insert_str(buffer, path, pos);
        pos += strlen(path);
    }
    if (i == 1) {
        insert_char(buffer, 0, '/');
    }
}

void init_vfs() {
    for (int i = 0; i < 26; i++) {
        vfsstl[i].flag = 0;
        vfsstl[i].disk_number = 0;
        vfsstl[i].drive = 0;
        vfsMount_Stl[i].flag = 0;
        vfsMount_Stl[i].disk_number = 0;
        vfsMount_Stl[i].drive = 0;
        // PDEBUG("Set vfsstl[%d] & vfsMount_Stl[%d] OK.", i, i);
    }
    vfs_now = NULL;
}

bool vfs_register_fs(vfs_t vfs) {
    printf("Register file system: %s\n", vfs.FSName);
    vfs_t *seat;
    seat = findSeat(vfsstl);
    if (!seat) {
        printf("can not find a seat of vfsstl(it's full)\n");
        return false;
    }
    *seat = vfs;
    return true;
}