#include "fatfs/diskio.h"

vfs_node_t fatfs_get_node_by_number(int number);
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(byte pdrv /* Physical drive nmuber to identify the drive */
) {
    DSTATUS stat = STA_NOINIT;
    // int result;
    if (fatfs_get_node_by_number(pdrv)) stat &= ~STA_NOINIT;
    return stat;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(byte pdrv /* Physical drive nmuber to identify the drive */
) {
    DSTATUS stat = STA_NOINIT;
    // int result;
    if (fatfs_get_node_by_number(pdrv)) stat &= ~STA_NOINIT;
    return stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(byte  pdrv,   /* Physical drive nmuber to identify the drive */
                  byte *buff,   /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  uint  count   /* Number of sectors to read */
) {
    DRESULT res = RES_PARERR;
    if (!fatfs_get_node_by_number(pdrv)) return RES_PARERR;
    vfs_read(fatfs_get_node_by_number(pdrv), buff, sector * 0x200, count * 0x200);
    res = RES_OK;
    return res;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(byte        pdrv,   /* Physical drive nmuber to identify the drive */
                   const byte *buff,   /* Data to be written */
                   LBA_t       sector, /* Start sector in LBA */
                   uint        count   /* Number of sectors to write */
) {
    DRESULT res = RES_PARERR;
    if (!fatfs_get_node_by_number(pdrv)) return RES_PARERR;
    vfs_write(fatfs_get_node_by_number(pdrv), (void *)buff, sector * 0x200, count * 0x200);
    res = RES_OK;
    return res;
}

#endif

extern vfs_node_t drive_number_mapping[10];

u32 Fdisk_size(byte drive) {
    return (u32)drive_number_mapping[drive]->size;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(byte  pdrv, /* Physical drive nmuber (0..) */
                   byte  cmd,  /* Control code */
                   void *buff  /* Buffer to send/receive control data */
) {
    // DRESULT res;
    // int result;

    switch (cmd) {
    case GET_SECTOR_SIZE: *(u16 *)buff = 512; return RES_OK;
    case GET_SECTOR_COUNT: *(u32 *)buff = Fdisk_size(pdrv + 0x41); return RES_OK;
    case GET_BLOCK_SIZE: *(u16 *)buff = 0; return RES_OK;
    case CTRL_SYNC: return RES_OK;
    default: break;
    }

    return RES_PARERR;
}
