#ifndef CRASHPOWEROS_SB16_H
#define CRASHPOWEROS_SB16_H

#define SB_MIXER      0x224 // DSP 混合器端口
#define SB_MIXER_DATA 0x225 // DSP 混合器数据端口
#define SB_RESET      0x226 // DSP 重置
#define SB_READ       0x22A // DSP 读
#define SB_WRITE      0x22C // DSP 写
#define SB_STATE      0x22E // DSP 读状态
#define SB_INTR16     0x22F // DSP 16 位中断响应

#define CMD_STC  0x40 // Set Time Constant
#define CMD_SOSR 0x41 // Set Output Sample Rate
#define CMD_SISR 0x42 // Set Input Sample Rate

#define CMD_SINGLE_IN8   0xC8 // Transfer mode 8bit input
#define CMD_SINGLE_OUT8  0xC0 // Transfer mode 8bit output
#define CMD_SINGLE_IN16  0xB8 // Transfer mode 16bit input
#define CMD_SINGLE_OUT16 0xB0 // Transfer mode 16bit output

#define CMD_AUTO_IN8   0xCE // Transfer mode 8bit input auto
#define CMD_AUTO_OUT8  0xC6 // Transfer mode 8bit output auto
#define CMD_AUTO_IN16  0xBE // Transfer mode 16bit input auto
#define CMD_AUTO_OUT16 0xB6 // Transfer mode 16bit output auto

#define CMD_ON      0xD1 // Turn speaker on
#define CMD_OFF     0xD3 // Turn speaker off
#define CMD_SP8     0xD0 // Stop playing 8 bit channel
#define CMD_RP8     0xD4 // Resume playback of 8 bit channel
#define CMD_SP16    0xD5 // Stop playing 16 bit channel
#define CMD_RP16    0xD6 // Resume playback of 16 bit channel
#define CMD_VERSION 0xE1 // Turn speaker off

#define MODE_MONO8    0x00
#define MODE_STEREO8  0x20
#define MODE_MONO16   0x10
#define MODE_STEREO16 0x30

#define STATUS_READ  0x80 // read buffer status
#define STATUS_WRITE 0x80 // write buffer status

#define DMA_BUF_SIZE 0x8000 // 缓冲区长度
#define SAMPLE_RATE 44100 // 采样率
#define SB16_IRQ    5

#include "task.h"
#include "common.h"
#include "isr.h"

struct WAV16_HEADER {
    char riff[4];
    int size;
    char wave[4];
    char fmt[4];
    int fmt_size;
    short format;
    short channel;
    int sample_rate;
    int byte_per_sec;
} __attribute__((packed));

struct sb16 {
    struct task_struct *use_task;
    int             status;
    char           *addr1;   // DMA 地址
    volatile size_t size1;   //
    char           *addr2;   // DMA 地址
    volatile size_t size2;   //
    uint8_t         mode;    // 模式
    uint8_t         channel; // DMA 通道
};

void sb16_handler(registers_t *reg);
void sb_exch_dmaaddr();
void sb16_do_dma();
void sb16_do_close();
void disable_sb16();
void sb16_close();
int sb16_write(char *data, size_t size);
void sb16_open();
void sb16_set_volume(uint8_t level);

#endif
