#include "../include/sb16.h"
#include "../include/io.h"
#include "../include/printf.h"
#include "../include/timer.h"
#include "../include/dma.h"
#include "../include/file.h"

struct sb16 sb;

void sb16_handler(registers_t *reg) {
    io_in8(SB_INTR16);
    uint8_t state = io_in8(SB_STATE);
    logkf("sb16 handler state 0x%X...\n", state);
    sb.flag = !sb.flag;
}

void disable_sb16() {
    sb.addr = (char*)DMA_BUF_ADDR;
    sb.mode = MODE_STEREO16;
    sb.channel = 5;
    sb.use_task = NULL;
    register_interrupt_handler(SB16_IRQ + 0x20, sb16_handler);
}

static void sb_reset() {
    io_out8(SB_RESET, 1);
    sleep(1);
    io_out8(SB_RESET, 0);
    uint8_t state = io_in8(SB_READ);
    logkf("sb16 reset state 0x%x\n", state);
}

static void sb_intr_irq() {
    io_out8(SB_MIXER, 0x80);
    uint8_t data = io_in8(SB_MIXER_DATA);
    if (data != 2) {
        io_out8(SB_MIXER, 0x80);
        io_out8(SB_MIXER_DATA, 0x2);
    }
}

static void sb_out(uint8_t cmd) {
    while (io_in8(SB_WRITE) & 128)
        ;
    io_out8(SB_WRITE, cmd);
}

static void sb_turn(bool on) {
    if (on)
        sb_out(CMD_ON);
    else
        sb_out(CMD_OFF);
}

static uint32_t sb_time_constant(uint8_t channels, uint16_t sample) {
    return 65536 - (256000000 / (channels * sample));
}

static void sb_set_volume(uint8_t level) {
    logkf("set sb16 volume to 0x%02X\n", level);
    io_out8(SB_MIXER, 0x22);
    io_out8(SB_MIXER_DATA, level);
}

int sb16_set(int cmd, void* args) {
    switch (cmd) {
        // 设置 tty 参数
        case 0:
            while (sb.use_task)
                ;
            io_cli();
            sb.use_task = get_current();
            io_sti();
            sb_reset();      // 重置 DSP
            sb_intr_irq();   // 设置中断
            sb_out(CMD_ON);  // 打开声霸卡
            return 0;
        case 1:
            sb_out(CMD_OFF);  // 关闭声霸卡
            sb.use_task = NULL;
            return 0;
        case 2:
            sb.mode = MODE_MONO8;
            sb.channel = 1;
            return 0;
        case 3:
            sb.mode = MODE_STEREO16;
            sb.channel = 5;
            return 0;
        case 4:
            sb_set_volume((uint32_t)args & 0xff);
            return 0;
        default:
            break;
    }
    return -1;
}

int sb16_write(char* data, size_t size) {
    memcpy(sb.addr, data, size);
    // 设置采样率
    sb_out(CMD_SOSR);                   // 44100 = 0xAC44
    sb_out((SAMPLE_RATE >> 8) & 0xFF);  // 0xAC
    sb_out(SAMPLE_RATE & 0xFF);         // 0x44
    dma_xfer(sb.channel, sb.addr, size, 0);
    if (sb.mode == MODE_MONO8) {
        sb_out(CMD_SINGLE_OUT8);
        sb_out(MODE_MONO8);
    } else {
        sb_out(CMD_SINGLE_OUT16);
        sb_out(MODE_STEREO16);
        size >>= 2;  // size /= 4
    }

    sb_out((size - 1) & 0xFF);
    sb_out(((size - 1) >> 8) & 0xFF);
    sb.flag = 0;
    //while (!sb.flag);
    return size;
}

void wav_player(char* filename) {
    FILE* fp = fopen(filename, "rb");
    if(fp == NULL) {
        logk("Open wav file was throw error.\n");
        return;
    }
    char* buf = kmalloc(DMA_BUF_SIZE);
    fread(buf, 1, 44, fp);
    logk("SB3 I\n");
    sb16_set(0, 0);
    logk("SB3 II\n");
    sb16_set(2, 0);
    logk("SB3 III\n");
    sb16_set(4, 0xff);
    logk("SB3 IV\n");
    int sz;
    while (sz = fread(buf, 1, DMA_BUF_SIZE, fp)) {
        logkf("%02x ",sz);
        sb16_write(buf, sz);
    }
    logk("\nSB3 IIV\n");
    sb16_set(1, 0);
    logk("SB3 IIIV\n");
    fclose(fp);
}