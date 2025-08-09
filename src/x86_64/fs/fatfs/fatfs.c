#include "errno.h"
#include "fatfs/ff.h"
#include "kprint.h"
#include "krlibc.h"
#include "sprintf.h"

static FATFS volume[10];
static int   fatfs_id = 0;
typedef struct file {
    char *path;
    void *handle;
} *file_t;

vfs_node_t drive_number_mapping[10] = {NULL};

static int alloc_number() {
    for (int i = 0; i < 10; i++)
        if (drive_number_mapping[i] == NULL) return i;
    printk("No available drive number");
    return -1;
}

vfs_node_t fatfs_get_node_by_number(int number) {
    if (number < 0 || number >= 10) return NULL;
    return drive_number_mapping[number];
}

int fatfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FRESULT res = f_mkdir(new_path);
    free(new_path);
    if (res != FR_OK) { return -1; }
    return 0;
}

int fatfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FIL     fp;
    FRESULT res = f_open(&fp, new_path, FA_CREATE_NEW);
    f_close(&fp);
    free(new_path);
    if (res != FR_OK) { return -1; }
    return 0;
}

size_t fatfs_readfile(file_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    FRESULT res;
    res = f_lseek(file->handle, offset);
    if (res != FR_OK) return -1;
    uint32_t n;
    res = f_read(file->handle, addr, size, &n);
    if (res != FR_OK) return -1;
    return n;
}

size_t fatfs_writefile(file_t file, const void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return -1;
    FRESULT res;
    res = f_lseek(file->handle, offset);
    if (res != FR_OK) return -1;
    uint32_t n;
    res = f_write(file->handle, addr, size, &n);
    if (res != FR_OK) return -1;
    f_sync(file->handle);
    return n;
}

static uint64_t ino = 2;

void fatfs_open(void *parent, const char *name, vfs_node_t node) {
    file_t p        = parent;
    char  *new_path = malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    file_t new      = malloc(sizeof(struct file));
    sprintf(new_path, "%s/%s", p->path, name);
    void   *fp = NULL;
    FILINFO fno;
    FRESULT res = f_stat(new_path, &fno);
    if (fno.fattrib & AM_DIR) {
        // node.
        node->type = file_dir;
        fp         = malloc(sizeof(DIR));
        res        = f_opendir(fp, new_path);
        for (;;) {
            // 读取目录下的内容，再读会自动读下一个文件
            res = f_readdir(fp, &fno);
            // 为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            bool has_equre = false;
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = child_node0->data;
                if (strcmp(e_child->name, fno.fname) == 0) {
                    has_equre = true;
                    break;
                }
            }
            if (has_equre) continue;
            vfs_node_t child_node = vfs_child_append(node, fno.fname, NULL);
            child_node->type      = ((fno.fattrib & AM_DIR) != 0) ? file_dir : file_none;
            child_node->inode     = ino++;
            child_node->size      = fno.fsize;
        }
        if (node->inode == 0) node->inode = ino++;
        node->blksz = PAGE_SIZE;
    } else {
        node->type = file_none;
        fp         = malloc(sizeof(FIL));
        res        = f_open(fp, new_path, FA_READ | FA_WRITE);
        if (node->inode == 0) node->inode = ino++;
        node->size  = f_size((FIL *)fp);
        node->blksz = PAGE_SIZE;
    }

    new->handle  = fp;
    new->path    = new_path;
    node->handle = new;
}

bool fatfs_close(file_t handle) {
    FILINFO fno;
    FRESULT res = f_stat(handle->path, &fno);
    if (fno.fattrib & AM_DIR) {
        res = f_closedir(handle->handle);
    } else {
        res = f_close(handle->handle);
    }
    if (res != FR_OK) return false;
    free(handle->path);
    free(handle->handle);
    free(handle);

    return true;
}

int fatfs_mount(const char *src, vfs_node_t node) {
    // if (node == rootdir) return -1; // 不支持fatfs作为rootfs
    if (src == DEVFS_REGISTER_ID || ((uint64_t)src) == MODFS_REGISTER_ID ||
        ((uint64_t)src) == TMPFS_REGISTER_ID || ((uint64_t)src) == PIEFS_REGISTER_ID)
        return VFS_STATUS_FAILED;

    int drive                   = alloc_number();
    drive_number_mapping[drive] = vfs_open(src);
    char *path                  = malloc(3);
    sprintf(path, "%d:", drive);
    FRESULT r = f_mount(&volume[drive], path, 1);
    if (r != FR_OK) {
        vfs_close(drive_number_mapping[drive]);
        drive_number_mapping[drive] = NULL;
        free(path);
        return -1;
    }
    file_t f = malloc(sizeof(struct file));
    f->path  = path;
    DIR *h   = malloc(sizeof(DIR));
    f_opendir(h, path);
    f->handle   = h;
    node->fsid  = fatfs_id;
    node->inode = 1;

    FILINFO fno;
    FRESULT res;
    for (;;) {
        // 读取目录下的内容，再读会自动读下一个文件
        res = f_readdir(h, &fno);
        // 为空时表示所有项目读取完毕，跳出
        if (res != FR_OK || fno.fname[0] == 0) break;
        vfs_node_t child_node = vfs_child_append(node, (const char *)fno.fname, NULL);
        child_node->type      = ((fno.fattrib & AM_DIR) != 0) ? file_dir : file_none;
        child_node->inode     = ino++;
    }
    // node->inode  = ino++;
    node->handle = f;
    // node->blksz  = DEFAULT_PAGE_SIZE;
    return VFS_STATUS_SUCCESS;
}

void fatfs_unmount(void *root) {
    file_t f                     = root;
    int    number                = f->path[0] - '0';
    drive_number_mapping[number] = NULL;
    f_closedir(f->handle);
    f_unmount(f->path);
    free(f->path);
    free(f->handle);
    free(f);
}

int fatfs_stat(void *handle, vfs_node_t node) {
    file_t  f = handle;
    FILINFO fno;
    FRESULT res = f_stat(f->path, &fno);
    if (res != FR_OK) return -1;
    if (fno.fattrib & AM_DIR) {
        node->type = file_dir;
    } else {
        node->type = file_none;
        node->size = fno.fsize;
        // node->createtime = fno.ftime
    }

    return 0;
}

int fatfs_delete(file_t parent, vfs_node_t node) {
    file_t file = node->handle;

    FRESULT res = f_unlink(file->path);
    if (res != FR_OK) return -1;
    return 0;
}

int fatfs_rename(file_t file, const char *new) {
    FRESULT res = f_rename((const char *)file->path, new);
    if (res != FR_OK) return -1;
    return 0;
}

int fatfs_ioctl(void *file, size_t cmd, void *arg) {
    return -EOPNOTSUPP;
}

int fatfs_poll(void *file, size_t events) {
    return -EOPNOTSUPP;
}

void *fatfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map((vfs_read_t)fatfs_readfile, file, (uint64_t)addr, size, prot, flags, offset);
}

vfs_node_t fatfs_dup(vfs_node_t node) {
    vfs_node_t copy   = vfs_node_alloc(node->parent, node->name);
    file_t     src    = node->handle;
    file_t     tar    = malloc(sizeof(struct file));
    tar->path         = strdup(src->path);
    tar->handle       = src->handle;
    copy->handle      = tar;
    copy->type        = node->type;
    copy->size        = node->size;
    copy->linkname    = node->linkname == NULL ? NULL : strdup(node->linkname);
    copy->flags       = node->flags;
    copy->permissions = node->permissions;
    copy->owner       = node->owner;
    copy->child       = node->child;
    copy->realsize    = node->realsize;
    copy->inode       = node->inode;
    return copy;
}

static struct vfs_callback fatfs_callbacks = {
    .mount   = fatfs_mount,
    .unmount = fatfs_unmount,
    .open    = fatfs_open,
    .close   = (vfs_close_t)fatfs_close,
    .read    = (vfs_read_t)fatfs_readfile,
    .write   = (vfs_write_t)fatfs_writefile,
    .mkdir   = fatfs_mkdir,
    .mkfile  = fatfs_mkfile,
    .delete  = (vfs_del_t)fatfs_delete,
    .rename  = (vfs_rename_t)fatfs_rename,
    .map     = (vfs_mapfile_t)fatfs_map,
    .stat    = fatfs_stat,
    .ioctl   = fatfs_ioctl,
    .poll    = fatfs_poll,
    .dup     = fatfs_dup,
};

void fatfs_init() {
    fatfs_id = vfs_regist("fatfs", &fatfs_callbacks);
    if (fatfs_id == VFS_STATUS_FAILED) { kerror("Failed to register fat filesystem\n"); }
}
