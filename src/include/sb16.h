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

#define MODE_SMONO   (0b01 << 4)
#define MODE_SSTEREO (0b11 << 4)
#define MODE_UMONO   (0b00 << 4)
#define MODE_USTEREO (0b10 << 4)

#define STATUS_READ  0x80 // read buffer status
#define STATUS_WRITE 0x80 // write buffer status

#define DMA_BUF_SIZE 4096 // 缓冲区长度
#define SB16_IRQ 5

#include "task.h"
#include "common.h"
#include "isr.h"

static struct {
    struct task_struct *use_task;  //
    int    status;    //
    bool   auto_mode; //
#if VSOUND_RWAPI
    void *volatile addr1;  // DMA 地址
  volatile size_t size1; //
  void *volatile addr2;  // DMA 地址
  volatile size_t size2; //
#endif
    uint8_t mode;        // 模式
    uint8_t dma_channel; // DMA 通道
    uint8_t depth;       // 采样深度
    int  channels;    // 声道数
} sb;

#endif
