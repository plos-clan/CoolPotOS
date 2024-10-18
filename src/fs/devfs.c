#include "../include/devfs.h"
#include "../include/vdisk.h"

typedef struct stack_t{
    NODE** array;
    int    index;
    int    size;
} STACK;

typedef struct queue_t{
    NODE** array;
    int    head;
    int    tail;
    int    num;
    int    size;
} QUEUE;

static NODE* create_node(){
    NODE* q;
    q = (NODE*)kmalloc(sizeof (NODE));
    q->n_children = 0;
    q->level      = -1;
    q->children   = NULL;
    return q;
}

bool devfs_check(uint8_t disk_number){
    vdisk *disk = get_disk(disk_number);
    if(disk == NULL) return false;
    if(disk->flag == 4096 && (!strcmp(disk->DriveName,"cpos_devdisk"))){
        return true;
    } else return false;
}

void devfs_init(struct vfs_t *vfs, uint8_t disk_number){

}

void register_devfs(){
    vfs_t fs;
    fs.flag = 1;
    fs.cache = NULL;
    strcpy(fs.FSName, "DEVFS");

    fs.Check = devfs_check;
    fs.InitFs = devfs_init;

    vfs_register_fs(fs);
}