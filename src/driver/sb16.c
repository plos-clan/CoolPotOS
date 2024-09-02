/*
#include "../include/sb16.h"
#include "../include/io.h"
#include "../include/printf.h"
#include "../include/timer.h"
#include "../include/dma.h"
#include "../include/file.h"

static void *const DMA_BUF_ADDR1 = (void *)0x90000;                // 不能跨越 64K 边界
static void *const DMA_BUF_ADDR2 = (void *)0x90000 + DMA_BUF_SIZE; // 不能跨越 64K 边界

#define STAT_OFF     0 // 未开启
#define STAT_WAITING 1 // 等待第一个缓冲区
#define STAT_PAUSED  2 // 已暂停
#define STAT_PLAYING 3 // 正在播放音频
#define STAT_CLOSING 4 // 正在等待播放完毕并关闭

#if VSOUND_RWAPI
static void sb_exch_dmaaddr() {
  char *addr  = sb.addr1;
  sb.addr1    = sb.addr2;
  sb.addr2    = addr;
  size_t size = sb.size1;
  sb.size1    = sb.size2;
  sb.size2    = size;
}
#endif

static void sb_send(uint8_t cmd) {
    waitif(io_in8(SB_WRITE) & MASK(7));
    io_out8(SB_WRITE, cmd);
}

#if VSOUND_RWAPI
static void sb16_do_dma() {
  size_t dmasize;
  size_t len;
  if (sb.auto_mode) {
    dmasize = 2 * DMA_BUF_SIZE;
    len     = sb.depth == 8 ? DMA_BUF_SIZE : DMA_BUF_SIZE / 2;
    len     = sb.channels == 2 ? len / 2 : len;
    if (sb.status != STAT_WAITING) return;
  } else {
    dmasize = sb.size2;
    len     = sb.depth == 8 ? sb.size2 : sb.size2 / 2;
    len     = sb.channels == 2 ? len / 2 : len;
  }

  byte mode = (sb.auto_mode ? 16 : 0) | 0x48; // 0x48 为播放 0x44 为录音
  dma_start(mode, sb.dma_channel, sb.addr2, dmasize, sb.depth == 16);
  if (sb.auto_mode) {
    sb_send(sb.depth == 8 ? CMD_AUTO_OUT8 : CMD_AUTO_OUT16);
  } else {
    sb_send(sb.depth == 8 ? CMD_SINGLE_OUT8 : CMD_SINGLE_OUT16);
  }
  sb_send(sb.mode);

  sb_send((len - 1) & 0xff);
  sb_send(((len - 1) >> 8) & 0xff);
}
#endif

#if VSOUND_RWAPI
static void sb16_send_buffer() {
  if (sb.size1 == 0) return;
  sb_exch_dmaaddr();
  sb16_do_dma();
  if (sb.status == STAT_WAITING) sb.status = STAT_PLAYING;
}
#endif

static void sb16_do_close() {
    if (sb.auto_mode) {
        sb_send(sb.depth == 8 ? 0xD9 : 0xDA);
    } else {
        sb_send(CMD_OFF);
    }
    sb.use_task = null;
    sb.status   = STAT_OFF;
}

static vsound_t snd;

void sb16_handler(registers_t *reg) {

    io_in8(sb.depth == 16 ? SB_INTR16 : SB_STATE);

#if VSOUND_RWAPI
    sb.size2 = 0;
  if (sb.size1 == 0) sb.status = STAT_WAITING;
  sb16_send_buffer();
#else
    vsound_played(snd);
#endif

    if (sb.status == STAT_CLOSING) {
        sb16_do_close();
        return;
    }
}

void sb16_init() {
    sb.use_task = NULL;
    sb.status   = STAT_OFF;
    register_interrupt_handler(SB16_IRQ + 0x20, sb16_handler);
}

static void sb_reset() {
    io_out8(SB_RESET, 1);
    auto oldcnt = timerctl.count;
    waitif(timerctl.count <= oldcnt + 10);
    io_out8(SB_RESET, 0);
    uint8_t state = io_in8(SB_READ);
}

static void sb_intr_irq() {
    io_out8(SB_MIXER, 0x80);
    uint8_t data = io_in8(SB_MIXER_DATA);
    if (data != 2) {
        io_out8(SB_MIXER, 0x80);
        io_out8(SB_MIXER_DATA, 0x2);
    }
}

static void sb16_set_samplerate(uint16_t rate) {
    sb_send(CMD_SOSR);
    sb_send(rate >> 8);
    sb_send(rate & 0xff);
}

static void sb16_set_volume(double volume) {
    volume = max(0, min(volume, 1));
    klogd("set sb16 volume to %d%%", (int)(volume * 100));
    io_out8(SB_MIXER, 0x22);
    io_out8(SB_MIXER_DATA, volume * 255);
}

extern bool is_vbox;

static int sb16_open(vsound_t vsound) {
    uint16_t fmt      = vsound->fmt;
    uint16_t channels = vsound->channels;
    uint32_t rate     = vsound->rate;
    double volume   = vsound->volume;

    sb.use_task = get_current();

    sb_reset();      // 重置 DSP
    sb_intr_irq();   // 设置中断
    sb_send(CMD_ON); // 打开声卡

    if (fmt == SOUND_FMT_S16 || fmt == SOUND_FMT_U16) {
        sb.depth       = 16;
        sb.dma_channel = 5;
    } else if (fmt == SOUND_FMT_U8 || fmt == SOUND_FMT_S8) {
        sb.depth       = 8;
        sb.dma_channel = 1;
    }
    if (channels == 1) {
        sb.mode = (fmt == SOUND_FMT_S16 || fmt == SOUND_FMT_S8) ? MODE_SMONO : MODE_UMONO;
    } else {
        sb.mode = (fmt == SOUND_FMT_S16 || fmt == SOUND_FMT_S8) ? MODE_SSTEREO : MODE_USTEREO;
    }
    sb.status = STAT_WAITING;
#if VSOUND_RWAPI
    sb.addr1 = DMA_BUF_ADDR1;
  sb.addr2 = DMA_BUF_ADDR2;
  sb.size1 = 0;
  sb.size2 = 0;
#endif
    sb.channels  = channels;
    sb.auto_mode = is_vbox ? true : false;

    sb16_set_samplerate(rate);
    sb16_set_volume(volume);

    return 0;
}

static void sb16_close(vsound_t vsound) {
    if (sb.status == STAT_PLAYING) {
        sb.status = STAT_CLOSING;
    } else {
        sb16_do_close();
    }
}

#if VSOUND_RWAPI
static int sb16_write(vsound_t vsound, const void *data, size_t len) {
  size_t size = len * vsound->settings.bytes_per_sample;
  while (size > 0) {
    waitif(sb.size1 == DMA_BUF_SIZE);
    asm_cli;
    size_t nwrite = min(size, DMA_BUF_SIZE - sb.size1);
    memcpy(sb.addr1 + sb.size1, data, nwrite);
    sb.size1 += nwrite;
    data     += nwrite;
    size     -= nwrite;
    asm_sti;
    if (sb.auto_mode) {
      if (sb.status == STAT_WAITING && sb.size1 == DMA_BUF_SIZE) sb16_send_buffer();
    } else {
      if (sb.status == STAT_WAITING) sb16_send_buffer();
    }
  }
  return 0;
}
#endif

#if !VSOUND_RWAPI
static int sb16_start_dma(vsound_t vsound, void *addr) {
    size_t dmasize;
    size_t len;
    if (sb.auto_mode) {
        dmasize = 2 * DMA_BUF_SIZE;
        len     = DMA_BUF_SIZE / vsound->bytes_per_sample;
        if (sb.status != STAT_WAITING) return 0;
    } else {
        dmasize = DMA_BUF_SIZE;
        len     = DMA_BUF_SIZE / vsound->bytes_per_sample;
    }

    byte mode = (sb.auto_mode ? 16 : 0) | 0x48; // 0x48 为播放 0x44 为录音
    dma_start(mode, sb.dma_channel, addr, dmasize, sb.depth == 16);
    if (sb.auto_mode) {
        sb_send(sb.depth == 8 ? CMD_AUTO_OUT8 : CMD_AUTO_OUT16);
    } else {
        sb_send(sb.depth == 8 ? CMD_SINGLE_OUT8 : CMD_SINGLE_OUT16);
    }
    sb_send(sb.mode);

    sb_send((len - 1) & 0xff);
    sb_send(((len - 1) >> 8) & 0xff);

    if (sb.status == STAT_WAITING) sb.status = STAT_PLAYING;
    return 0;
}
#endif

static struct vsound vsound = {
        .is_output = true,
#if VSOUND_RWAPI
        .is_rwmode = true,
#endif
        .name  = "sb16",
        .open  = sb16_open,
        .close = sb16_close,
#if VSOUND_RWAPI
        .write = sb16_write,
#else
        .start_dma = sb16_start_dma,
        .bufsize   = DMA_BUF_SIZE,
#endif
};

static const i16 fmts[] = {
        SOUND_FMT_S8, SOUND_FMT_U8, SOUND_FMT_S16, SOUND_FMT_U16, -1,
};

static const i32 rates[] = {
        8000, 11025, 16000, 22050, 24000, 32000, 44100, 47250, 48000, 50000, -1,
};

void sb16_regist() {
    snd = &vsound;
    if (!vsound_regist(snd)) {
        klogw("注册 sb16 失败");
        return;
    }
    vsound_set_supported_fmts(snd, fmts, -1);
    vsound_set_supported_rates(snd, rates, -1);
    vsound_set_supported_ch(snd, 1);
    vsound_set_supported_ch(snd, 2);
#if !VSOUND_RWAPI
    vsound_addbuf(snd, DMA_BUF_ADDR1);
    vsound_addbuf(snd, DMA_BUF_ADDR2);
#endif
}

 */