#include "../include/floppy.h"
#include "../include/task.h"
#include "../include/printf.h"
#include "../include/vdisk.h"
#include "../include/io.h"
#include "../include/dma.h"

struct task_struct *floppy_use;
struct task_struct *waiter;
volatile int floppy_int_count = 0;

static volatile int done = 0;
static int dchange = 0;
static int motor = 0;
static int mtick = 0;
static volatile int tmout = 0;
static unsigned char status[7] = {0};
static unsigned char statsz = 0;
static unsigned char sr0 = 0;
static unsigned char fdc_track = 0xff;
static DrvGeom geometry = {DG144_HEADS, DG144_TRACKS, DG144_SPT};
unsigned long tbaddr = 0x80000L;

static void Read(char drive, unsigned char *buffer, unsigned int number,
                 unsigned int lba) {
    floppy_use = get_current();
    for (int i = 0; i < number; i += SECTORS_ONCE) {
        int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
        fdc_rw(lba + i, buffer + i * 512, 1, sectors);
    }
    floppy_use = NULL;
}

static void Write(char drive, unsigned char *buffer, unsigned int number,
                  unsigned int lba) {
    floppy_use = get_current();
    for (int i = 0; i < number; i += SECTORS_ONCE) {
        int sectors = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
        fdc_rw(lba + i, buffer + i * 512, 0, sectors);
    }
    floppy_use = NULL;
}

void init_floppy() {
    sendbyte(CMD_VERSION);  //发送命令（获取软盘版本），如果收到回应，说明软盘正在工作
    if (getbyte() == -1) {
        printf("floppy: no floppy drive found\n");
        printf("No fount FDC\n");
        return;
    }

    register_interrupt_handler(0x26,flint);

    printf("[\035floppy\036]: FLOPPY DISK:RESETING\n");
    reset();  //重置软盘驱动器
    printf("[\035floppy\036]:FLOPPY DISK:reset over!\n");
    sendbyte(CMD_VERSION);                //获取软盘版本
    printf("[\035floppy\036]:FDC_VER:0x%x\n", getbyte());  //并且输出到屏幕上
    vdisk vd;
    strcpy(vd.DriveName, "floppy");
    vd.Read = Read;
    vd.Write = Write;
    vd.size = 1474560;
    vd.flag = 1;
    register_vdisk(vd);
}

int fdc_rw_ths(int track,
               int head,
               int sector,
               unsigned char* blockbuff,
               int read,
               unsigned long nosectors) {
    // 跟上面的大同小异
    int tries, copycount = 0;
    unsigned char* p_tbaddr = (char*)0x80000;
    unsigned char* p_blockbuff = blockbuff;

    motoron();

    if (!read && blockbuff) {
        for (copycount = 0; copycount < (nosectors * 512); copycount++) {
            *p_tbaddr = *p_blockbuff;
            p_blockbuff++;
            p_tbaddr++;
        }
    }

    for (tries = 0; tries < 3; tries++) {
        if (io_in8(FDC_DIR) & 0x80) {
            dchange = 1;
            seek(1);
            recalibrate();
            motoroff();

            return fdc_rw_ths(track, head, sector, blockbuff, read, nosectors);
        }
        if (!seek(track)) {
            motoroff();
            return 0;
        }

        io_out8(FDC_CCR, 0);

        if (read) {
            dma_xfer(2, tbaddr, nosectors * 512, 0);
            sendbyte(CMD_READ);
        } else {
            dma_xfer(2, tbaddr, nosectors * 512, 1);
            sendbyte(CMD_WRITE);
        }

        sendbyte(head << 2);
        sendbyte(track);
        sendbyte(head);
        sendbyte(sector);
        sendbyte(2);
        sendbyte(geometry.spt);

        if (geometry.spt == DG144_SPT)
            sendbyte(DG144_GAP3RW);
        else
            sendbyte(DG168_GAP3RW);
        sendbyte(0xff);

        wait_floppy_interrupt();

        if ((status[0] & 0xc0) == 0)
            break;

        recalibrate();
    }

    motoroff();

    if (read && blockbuff) {
        p_blockbuff = blockbuff;
        p_tbaddr = (char*)0x80000;
        for (copycount = 0; copycount < (nosectors * 512); copycount++) {
            *p_blockbuff = *p_tbaddr;
            p_blockbuff++;
            p_tbaddr++;
        }
    }

    return (tries != 3);
}

int write_floppy_for_ths(int track,
                         int head,
                         int sec,
                         unsigned char* blockbuff,
                         unsigned long nosec) {
    int res = fdc_rw_ths(track, head, sec, blockbuff, 0, nosec);
}

void flint(registers_t *reg) {
    floppy_int_count =
            1;  // 设置中断计数器为1，代表中断已经发生（或者是系统已经收到了中断）
    io_out8(0x20, 0x20);  // 发送EOI信号，告诉PIC，我们已经处理完了这个中断
    //task_run(waiter);
    // task_next();
}

void sendbyte(int byte) {
    volatile int msr;
    int tmo;  // 超时计数器

    for (tmo = 0; tmo < 128; tmo++)  // 这里我们只给128次尝试的机会
    {
        msr = io_in8(FDC_MSR);
        if ((msr & 0xc0) ==
            0x80) {
            io_out8(FDC_DATA, byte);
            return;
        }
        io_in8(0x80); /* 等待 */
    }
}

int getbyte() {
    int msr;  // 软盘驱动器状态寄存器
    int tmo;  // 软盘驱动器状态寄存器的超时计数器

    for (tmo = 0; tmo < 128; tmo++){
        msr = io_in8(FDC_MSR);
        if ((msr & 0xd0) == 0xd0){
            return io_in8(FDC_DATA);
        }
        io_in8(0x80); /* 延时 */
    }
    return -1; /* 没读取到 */
}

void set_waiter(struct task_struct *t) {
    while(waiter); // wait
    waiter = t;
}

void reset(void) {
    set_waiter(get_current());
    io_out8(FDC_DOR, 0);

    mtick = 0;
    motor = 0;

    io_out8(FDC_DRS, 0);

    io_out8(FDC_DOR, 0x0c);

    wait_floppy_interrupt();
    sendbyte(CMD_SPECIFY);
    sendbyte(0xdf); /* SRT = 3ms, HUT = 240ms */
    sendbyte(0x02); /* HLT = 16ms, ND = 0 */

    recalibrate();

    dchange =
            0;  //清除“磁盘更改”状态（将dchange设置为false，让别的函数知道磁盘更改状态已经被清楚了）
}
void motoron(void) {
    if (!motor) {
        mtick = -1; /* 停止电机计时 */
        io_out8(FDC_DOR, 0x1c);
        for (int i = 0; i < 80000; i++)
            ;
        motor = 1;  //设置电机状态为true
    }
}

/* 关闭电机 */
void motoroff(void) {
    if (motor) {
        mtick = 13500; /* 重新初始化电机计时器 */
    }
}

/* 重新校准驱动器 */
void recalibrate() {
    set_waiter(get_current());
    /* 先启用电机 */
    motoron();

    /* 然后重新校准电机 */
    sendbyte(CMD_RECAL);
    sendbyte(0);

    /* 等待软盘中断（也就是电机校准成功） */
    wait_floppy_interrupt();
    /* 关闭电机 */
    motoroff();
}

void wait_floppy_interrupt() {
    io_sti();
    while(!floppy_int_count);
    statsz = 0;  // 清空状态
    while ((statsz < 7) &&(io_in8(FDC_MSR) &(1<< 4)))  {
        status[statsz++] = getbyte();  // 获取数据
    }

    /* 获取中断状态 */
    sendbyte(CMD_SENSEI);
    sr0 = getbyte();
    fdc_track = getbyte();

    floppy_int_count = 0;
    floppy_use = NULL;
    return;
}

int seek(int track) {
    if (fdc_track == track) /* 目前的磁道和需要seek的磁道一样吗 */
    {
        // 一样的话就不用seek了
        return 1;
    }
    set_waiter(get_current());
    /* 向软盘控制器发送SEEK命令 */
    sendbyte(CMD_SEEK);
    sendbyte(0);
    sendbyte(track); /* 要seek到的磁道号 */

    /* 发送完之后，软盘理应会送来一个中断 */
    wait_floppy_interrupt();  // 所以我们需要等待软盘中断

    for (int i = 0; i < 500; i++)
        ;
    /* 检查一下成功了没有 */
    if ((sr0 != 0x20) || (fdc_track != track))
        return 0;  // 没成功
    else
        return 1;  // 成功了
}

void block2hts(int block, int* track, int* head, int* sector) {
    *track = (block / 18) / 2;
    *head = (block / 18) % 2;
    *sector = block % 18 + 1;
}

void hts2block(int track, int head, int sector, int* block) {
    *block = track * 18 * 2 + head * 18 + sector;
}

int fdc_rw(int block,
           unsigned char* blockbuff,
           int read,
           unsigned long nosectors) {
    set_waiter(get_current());
    int head, track, sector, tries, copycount = 0;
    unsigned char* p_tbaddr =
            (char*)0x80000;  // 512byte
    // 缓冲区（大家可以放心，我们再page.c已经把这里设置为占用了）
    unsigned char* p_blockbuff = blockbuff;  // r/w的数据缓冲区

    /* 获取block对应的ths */
    block2hts(block, &track, &head, &sector);

    motoron();

    if (!read && blockbuff) {
        /* 从数据缓冲区复制数据到轨道缓冲区 */
        for (copycount = 0; copycount < (nosectors * 512); copycount++) {
            *p_tbaddr = *p_blockbuff;
            p_blockbuff++;
            p_tbaddr++;
        }
    }

    for (tries = 0; tries < 3; tries++) {
        /* 检查 */
        if (io_in8(FDC_DIR) & 0x80) {
            waiter = NULL;
            dchange = 1;
            seek(1); /* 清除磁盘更改 */
            recalibrate();
            motoroff();
            return fdc_rw(block, blockbuff, read, nosectors);
        }
        waiter = NULL;
        /* seek到track的位置*/
        if (!seek(track)) {
            motoroff();
            waiter = NULL;
            return 0;
        }
        set_waiter(get_current());
        /* 传输速度（500K/s） */
        io_out8(FDC_CCR, 0);

        /* 发送命令 */
        if (read) {
            dma_xfer(2, tbaddr, nosectors * 512, 0);
            sendbyte(CMD_READ);
        } else {
            dma_xfer(2, tbaddr, nosectors * 512, 1);
            sendbyte(CMD_WRITE);
        }

        sendbyte(head << 2);
        sendbyte(track);
        sendbyte(head);
        sendbyte(sector);
        sendbyte(2); /* 每个扇区是512字节 */
        sendbyte(geometry.spt);

        if (geometry.spt == DG144_SPT)
            sendbyte(DG144_GAP3RW);
        else
            sendbyte(DG168_GAP3RW);
        sendbyte(0xff);

        /* 等待中断...... */
        /* 读写数据不需要中断状态 */
        wait_floppy_interrupt();

        if ((status[0] & 0xc0) == 0)
            break; /* worked! outta here! */
        waiter = NULL;
        recalibrate(); /* az，报错了，再试一遍 */
    }

    /* 关闭电动机 */
    motoroff();

    if (read && blockbuff) {
        /* 复制数据 */
        p_blockbuff = blockbuff;
        p_tbaddr = (char*)0x80000;
        for (copycount = 0; copycount < (nosectors * 512); copycount++) {
            *p_blockbuff = *p_tbaddr;
            p_blockbuff++;
            p_tbaddr++;
        }
    }

    return (tries != 3);
}
