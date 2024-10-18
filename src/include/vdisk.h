#ifndef CRASHPOWEROS_VDISK_H
#define CRASHPOWEROS_VDISK_H

#define SECTORS_ONCE 8

#include "common.h"

typedef struct {
    void (*Read)(char drive, unsigned char *buffer, unsigned int number,
                 unsigned int lba);
    void (*Write)(char drive, unsigned char *buffer, unsigned int number,
                  unsigned int lba);
    int flag;
    unsigned int size; // 大小
    char DriveName[50];
} vdisk;

struct IDEHardDiskInfomationBlock {
    char reserve1[2];
    unsigned short CylinesNum;
    char reserve2[2];
    unsigned short HeadersNum;
    unsigned short TrackBytes;
    unsigned short SectorBytes;
    unsigned short TrackSectors;
    char reserve3[6];
    char OEM[20];
    char reserve4[2];
    unsigned short BuffersBytes;
    unsigned short EECCheckSumLength;
    char Version[8];
    char ID[40];
};

int init_vdisk();
int register_vdisk(vdisk vd); //注册一个硬盘设备
int logout_vdisk(char drive);
int rw_vdisk(char drive, unsigned int lba, unsigned char *buffer,
             unsigned int number, int read);
int have_vdisk(char drive);

void Disk_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive); // 读取指定扇区 ATA设备 512字节
bool CDROM_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive); // 读取指定扇区 ATAPI设备 2048字节
unsigned int disk_Size(char drive);
int DiskReady(char drive); // 硬盘是否就绪
void Disk_Write(unsigned int lba, unsigned int number, void *buffer,
                char drive);
unsigned int GetDriveCode(unsigned char *name);
bool SetDrive(unsigned char *name);
void Disk_Read2048(unsigned int lba, unsigned int number, void *buffer,
                   char drive);
void DriveSemaphoreGive(unsigned int drive_code);
bool DriveSemaphoreTake(unsigned int drive_code);
vdisk *get_disk(char drive);

#endif
