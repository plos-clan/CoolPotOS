#include "../include/sb16.h"
#include "../include/io.h"
#include "../include/printf.h"
#include "../include/timer.h"
#include "../include/dma.h"
#include "../include/file.h"

static void *const DMA_BUF_ADDR1 = (void *)0x90000;                // 不能跨越 64K 边界
static void *const DMA_BUF_ADDR2 = (void *)0x90000 + DMA_BUF_SIZE; // 不能跨越 64K 边界

struct sb16 sb;

void sb_exch_dmaaddr() {
    char *addr  = sb.addr1;
    sb.addr1    = sb.addr2;
    sb.addr2    = addr;
    size_t size = sb.size1;
    sb.size1    = sb.size2;
    sb.size2    = size;
}

static void sb_out(uint8_t cmd) {
    while (io_in8(SB_WRITE) & 128)
        ;
    io_out8(SB_WRITE, cmd);
}

void sb16_do_dma() {
    // 设置采样率
    sb_out(CMD_SOSR);                  // 44100 = 0xAC44
    sb_out((SAMPLE_RATE >> 8) & 0xFF); // 0xAC
    sb_out(SAMPLE_RATE & 0xFF);        // 0x44

    dma_xfer(sb.channel, (uint32_t)(sb.addr2), sb.size2, 0);
    if (sb.mode == MODE_MONO8) {
        sb_out(CMD_SINGLE_OUT8);
        sb_out(MODE_MONO8);
    } else {
        sb_out(CMD_SINGLE_OUT16);
        sb_out(MODE_STEREO16);
    }

    sb_out((sb.size2 - 1) & 0xFF);
    sb_out(((sb.size2 - 1) >> 8) & 0xFF);
}

void sb16_do_close() {
    sb_out(CMD_OFF); // 关闭声卡
    sb.use_task = NULL;
}

void sb16_handler(registers_t *reg) {
    io_in8(SB_INTR16);
    uint8_t state = io_in8(SB_STATE);

    io_cli();
    sb.size2 = 0;
    if (sb.size1 > 0) {
        sb_exch_dmaaddr();
        sb16_do_dma();
    }
    io_cli();

    if (sb.status > 0) {
        sb16_do_close();
        return;
    }
}

void disable_sb16() {
    sb.addr1    = DMA_BUF_ADDR1;
    sb.addr2    = DMA_BUF_ADDR2;
    sb.mode     = MODE_STEREO16;
    sb.channel  = 5;
    sb.use_task = NULL;
    sb.size1    = 0;
    sb.size2    = 0;
    sb.status   = 0;
    register_interrupt_handler(SB16_IRQ + 0x20, (uint32_t)sb16_handler);
}

static void sb_reset() {
    io_out8(SB_RESET, 1);
    sleep(10);
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

static void sb_turn(bool on) {
    if (on)
        sb_out(CMD_ON);
    else
        sb_out(CMD_OFF);
}

static uint32_t sb_time_constant(uint8_t channels, uint16_t sample) {
    return 65536 - (256000000 / (channels * sample));
}

void sb16_set_volume(uint8_t level) {
    logkf("set sb16 volume to %d/255\n", level);
    io_out8(SB_MIXER, 0x22);
    io_out8(SB_MIXER_DATA, level);
}

void sb16_open() {
    while (sb.use_task) {}
    io_cli();
    sb.use_task = get_current();
    io_sti();
    sb_reset();     // 重置 DSP
    sb_intr_irq();  // 设置中断
    sb_out(CMD_ON); // 打开声卡

    sb.mode    = MODE_MONO8;
    sb.channel = 1;
    sb.size1   = 0;
    sb.size2   = 0;
    sb.status  = 0;

    // sb.mode    = MODE_STEREO16;
    // sb.channel = 5;
}

void sb16_close() {
    sb.status = 1;
    if (sb.size2 > 0) return;
    sb16_do_close();
}

int sb16_write(char *data, size_t size) {
    while (sb.size1) {}

    memcpy(sb.addr1, data, size);
    sb.size1 = size;

    if (sb.size2 > 0) return size;

    sb_exch_dmaaddr();
    sb16_do_dma();

    return size;
}