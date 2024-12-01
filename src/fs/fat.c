#include "fat.h"
#include "fatfs/ff.h"
#include "vfs.h"
#include "krlibc.h"
#include "klog.h"

#define error(msg) printk("[FatErr]: %s\n",msg)

static FATFS      volume[10];
static vfs_node_t drive_number_mapping[10] = {NULL};
static int        fatfs_id                 = 0;
typedef struct file {
    char *path;
    void *handle;
} *file_t;

static int alloc_number() {
    for (int i = 0; i < 10; i++)
        if (drive_number_mapping[i] == NULL) return i;
    error("No available drive number");
    return -1;
}

vfs_node_t fatfs_get_node_by_number(int number) {
    if (number < 0 || number >= 10) return NULL;
    return drive_number_mapping[number];
}

int fatfs_mkdir(void *parent, const char* name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = kmalloc(strlen(p->path) + strlen(name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FRESULT res = f_mkdir(new_path);
    kfree(new_path);
    if (res != FR_OK) { return -1; }
    return 0;
}

int fatfs_mkfile(void *parent, const char* name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = kmalloc(strlen(p->path) + strlen(name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FIL     fp;
    FRESULT res = f_open(&fp, new_path, FA_CREATE_NEW);
    f_close(&fp);
    kfree(new_path);
    if (res != FR_OK) { return -1; }
    return 0;
}

int fatfs_readfile(file_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    FRESULT res;
    res = f_lseek(file->handle, offset);
    if (res != FR_OK) return -1;
    uint32_t n;
    res = f_read(file->handle, addr, size, &n);
    if (res != FR_OK) return -1;
    return 0;
}

int fatfs_writefile(file_t file, const void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    FRESULT res;
    res = f_lseek(file->handle, offset);
    if (res != FR_OK) return -1;
    uint32_t n;
    res = f_write(file->handle, addr, size, &n);
    if (res != FR_OK) return -1;
    return 0;
}

void fatfs_open(void *parent, const char* name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = kmalloc(strlen(p->path) + strlen(name) + 1 + 1);
    file_t new      = kmalloc(sizeof(struct file));
    sprintf(new_path, "%s/%s", p->path, name);
    void   *fp = NULL;
    FILINFO fno;
    FRESULT res = f_stat(new_path, &fno);
    assert(res == FR_OK, "f_stat %d %s", res, new_path);
    if (fno.fattrib & AM_DIR) {
        //node.
        node->type = file_dir;
        fp         = kmalloc(sizeof(DIR));
        res        = f_opendir(fp, new_path);
        assert(res == FR_OK);
        for (;;) {
            //读取目录下的内容，再读会自动读下一个文件
            res = f_readdir(fp, &fno);
            //为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            vfs_child_append(node, fno.fname, NULL);
        }
    } else {
        node->type = file_block;
        fp         = kmalloc(sizeof(FIL));
        res        = f_open(fp, new_path, FA_READ | FA_WRITE);
        node->size = f_size((FIL *)fp);
        assert(res == FR_OK);
    }

    assert(fp != NULL);
    new->handle  = fp;
    new->path    = new_path;
    node->handle = new;
}

void fatfs_close(file_t handle) {
    FILINFO fno;
    FRESULT res = f_stat(handle->path, &fno);
    assert(res == FR_OK);
    if (fno.fattrib & AM_DIR) {
        res = f_closedir(handle->handle);
    } else {
        res = f_close(handle->handle);
    }
    kfree(handle->path);
    kfree(handle->handle);
    kfree(handle);
    assert(res == FR_OK);
}

int fatfs_mount(const char* src, vfs_node_t node) {
    if (!src) return -1;
    int drive = alloc_number();
    assert(drive != -1);
    drive_number_mapping[drive] = vfs_open(src);
    assert(drive_number_mapping[drive] != NULL);
    char *path = kmalloc(3);
    bzero(path, 0);
    sprintf(path, "%d:", drive);
    FRESULT r = f_mount(&volume[drive], path, 1);
    if (r != FR_OK) {
        vfs_close(drive_number_mapping[drive]);
        drive_number_mapping[drive] = NULL;
        kfree(path);
        return -1;
    }
    file_t f = kmalloc(sizeof(struct file));
    f->path  = path;
    DIR *h   = kmalloc(sizeof(DIR));
    f_opendir(h, path);
    f->handle  = h;
    node->fsid = fatfs_id;

    FILINFO fno;
    FRESULT res;
    for (;;) {
        //读取目录下的内容，再读会自动读下一个文件
        res = f_readdir(h, &fno);
        //为空时表示所有项目读取完毕，跳出
        if (res != FR_OK || fno.fname[0] == 0) break;
        vfs_child_append(node, fno.fname, NULL);
    }
    node->handle = f;
    return 0;
}

void fatfs_unmount(void *root) {
    file_t f                     = root;
    int    number                = f->path[0] - '0';
    drive_number_mapping[number] = NULL;
    f_closedir(f->handle);
    f_unmount(f->path);
    kfree(f->path);
    kfree(f->handle);
    kfree(f);
}

int fatfs_stat(void *handle, vfs_node_t node) {
    file_t  f = handle;
    FILINFO fno;
    FRESULT res = f_stat(f->path, &fno);
    if (res != FR_OK) return -1;
    if (fno.fattrib & AM_DIR) {
        node->type = file_dir;
    } else {
        node->type = file_block;
        node->size = fno.fsize;
        // node->createtime = fno.ftime
    }
    return 0;
}

static struct vfs_callback callbacks = {
        .mount   = fatfs_mount,
        .unmount = fatfs_unmount,
        .open    = fatfs_open,
        .close   = (vfs_close_t)fatfs_close,
        .read    = (vfs_read_t)fatfs_readfile,
        .write   = (vfs_write_t)fatfs_writefile,
        .mkdir   = fatfs_mkdir,
        .mkfile  = fatfs_mkfile,
        .stat    = fatfs_stat,
};

void fatfs_regist() {
    fatfs_id = vfs_regist("fatfs", &callbacks);
}