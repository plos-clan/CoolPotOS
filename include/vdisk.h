#ifndef CRASHPOWEROS_VDISK_H
#define CRASHPOWEROS_VDISK_H

#define SECTORS_ONCE 8

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
int register_vdisk(vdisk vd);
int logout_vdisk(char drive);
int rw_vdisk(char drive, unsigned int lba, unsigned char *buffer,
             unsigned int number, int read);
int have_vdisk(char drive);
void Disk_Read(unsigned int lba, unsigned int number, void *buffer,
               char drive);
int disk_Size(char drive);
int DiskReady(char drive);
void Disk_Write(unsigned int lba, unsigned int number, void *buffer,
                char drive);

#endif
