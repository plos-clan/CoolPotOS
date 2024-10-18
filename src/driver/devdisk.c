#include "../include/devdisk.h"
#include "../include/vdisk.h"
#include "../include/memory.h"
#include "../include/printf.h"

static void devdisk_read(char drive, unsigned char *buffer, unsigned int number,unsigned int lba){
}

static void devdisk_write(char drive, unsigned char *buffer, unsigned int number,unsigned int lba){
}

void devdisk_init(){
    vdisk *disk = kmalloc(sizeof(vdisk));
    disk->flag = 4096;
    strcpy(disk->DriveName,"cpos_devdisk");
    disk->size = 0;
    disk->Read = devdisk_read;
    disk->Write = devdisk_write;
    register_vdisk(*disk);
    klogf(true,"Enable device virtual disk.\n");
}