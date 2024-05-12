#ifndef CRASHPOWEROS_FLOPPY_H
#define CRASHPOWEROS_FLOPPY_H

#define DG144_HEADS 2      /* 每个磁道中的磁头数 (1.44M) */
#define DG144_TRACKS 80    /* 每个驱动器的磁道数 (1.44M) */
#define DG144_SPT 18       /* 每个磁头中的的扇区数 (1.44M) */
#define DG144_GAP3FMT 0x54 /* GAP3格式化 (1.44M) */
#define DG144_GAP3RW 0x1b  /* GAP3（读/写） (1.44M) */

#define DG168_HEADS 2      /* 每个磁道中的磁头数 (1.68M) */
#define DG168_TRACKS 80    /* 每个驱动器的磁道数 (1.68M) */
#define DG168_SPT 21       /* 每个磁头中的的扇区数 (1.68M) */
#define DG168_GAP3FMT 0x0c /* GAP3格式化 (1.68M) */
#define DG168_GAP3RW 0x1c  /* GAP3（读/写） (1.68M) */

/* i/o端口定义 */
#define FDC_DOR (0x3f2)  /* 数字输出寄存器 */
#define FDC_MSR (0x3f4)  /* 主要状态寄存器（输入） */
#define FDC_DRS (0x3f4)  /* DRS寄存器 */
#define FDC_DATA (0x3f5) /* 数据寄存器 */
#define FDC_DIR (0x3f7)  /* 数字输入寄存器（输入） */
#define FDC_CCR (0x3f7)  /* CCR寄存器 */

/* 软盘命令 */
#define CMD_SPECIFY (0x03) /* 指定驱动器计时 */
#define CMD_WRITE (0xc5)   /* 写（写入数据的最小单位是扇区） */
#define CMD_READ (0xe6)    /* 读（读取扇区的最小单位是扇区） */
#define CMD_RECAL (0x07)   /* 重新校准软盘 */
#define CMD_SENSEI (0x08)  /* 中断状态 */
#define CMD_FORMAT (0x4d)  /* 格式化磁道 */
#define CMD_SEEK (0x0f)    /* 寻找磁道 */
#define CMD_VERSION (0x10) /* 获取软盘驱动器的版本 */
#define SECTORS_ONCE 4

#include "isr.h"

typedef struct DrvGeom {
    unsigned char heads;
    unsigned char tracks;
    unsigned char spt; /* 每轨扇区数 */
} DrvGeom;

int getbyte();
void sendbyte(int byte);
void motoron(void);
void motoroff(void);
void recalibrate(void);
void init_floppy();
void hts2block(int track, int head, int sector, int* block);
void block2hts(int block, int* track, int* head, int* sector);
int seek(int track);
void wait_floppy_interrupt();
void reset(void);
int fdc_rw(int block,
           unsigned char* blockbuff,
           int read,
           unsigned long nosectors);
void flint(registers_t *reg);
int write_floppy_for_ths(int track,
                         int head,
                         int sec,
                         unsigned char* blockbuff,
                         unsigned long nosec);
int fdc_rw_ths(int track,
               int head,
               int sector,
               unsigned char* blockbuff,
               int read,
               unsigned long nosectors);

#endif
