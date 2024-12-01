/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h" /* Declarations of disk functions */
#include "ff.h"     /* Obtains integer types */
#include "vfs.h"
#include "vdisk.h"

vfs_node_t fatfs_get_node_by_number(int number);
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(uint8_t pdrv /* Physical drive nmuber to identify the drive */
) {
  DSTATUS stat = STA_NOINIT;
  int     result;
  if (fatfs_get_node_by_number(pdrv)) stat &= ~STA_NOINIT;
  return stat;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(uint8_t pdrv /* Physical drive nmuber to identify the drive */
) {
  DSTATUS stat = STA_NOINIT;
  int     result;
  if (fatfs_get_node_by_number(pdrv)) stat &= ~STA_NOINIT;
  return stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(uint8_t  pdrv,   /* Physical drive nmuber to identify the drive */
                  uint8_t *buff,   /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  uint32_t  count   /* Number of sectors to read */
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

DRESULT disk_write(uint8_t        pdrv,   /* Physical drive nmuber to identify the drive */
                   const uint8_t *buff,   /* Data to be written */
                   LBA_t       sector, /* Start sector in LBA */
                   uint32_t        count   /* Number of sectors to write */
) {
  DRESULT res = RES_PARERR;
  if (!fatfs_get_node_by_number(pdrv)) return RES_PARERR;
  vfs_write(fatfs_get_node_by_number(pdrv), (void *)buff, sector * 0x200, count * 0x200);
  res = RES_OK;
  return res;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(uint8_t  pdrv, /* Physical drive nmuber (0..) */
                   uint8_t  cmd,  /* Control code */
                   void *buff  /* Buffer to send/receive control data */
) {
  extern vdisk vdisk_ctl[26];
  DRESULT      res;
  int          result;

  switch (cmd) {
  case GET_SECTOR_SIZE: *(uint16_t *)buff = 2048; return RES_OK;
  case GET_SECTOR_COUNT: *(uint32_t *)buff = disk_Size(pdrv + 0x41); return RES_OK;
  case GET_BLOCK_SIZE: *(uint16_t *)buff = 0; return RES_OK;
  default: break;
  }

  return RES_PARERR;
}
