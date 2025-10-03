#pragma once

#define QUEUE_IMPLEMENTATION

#include "ctype.h"
#include "krlibc.h"
#include "queue.h"

#ifndef __cplusplus
typedef enum {
#    define FMT(f) SOUND_FMT_##f
#else
enum class SoundFmt : i32 {
#    define FMT(f) f
#endif
    FMT(ukn) = -1,
    // - 8bit / 1byte
    FMT(S8),
    FMT(U8),
    // - 16bit / 2byte
    FMT(S16L),
    FMT(S16B),
    FMT(U16L),
    FMT(U16B),
    // - 24bit / 3byte
    FMT(S24L),
    FMT(S24B),
    FMT(U24L),
    FMT(U24B),
    // - 24bit / 4byte (low 3byte)
    FMT(S24L32),
    FMT(S24B32),
    FMT(U24L32),
    FMT(U24B32),
    // - 32bit / 4byte
    FMT(S32L),
    FMT(S32B),
    FMT(U32L),
    FMT(U32B),
    // - 64bit / 8byte
    FMT(S64L),
    FMT(S64B),
    FMT(U64L),
    FMT(U64B),
    // - 16bit / 2byte  <- float
    FMT(F16L),
    FMT(F16B),
    // - 32bit / 4byte  <- float
    FMT(F32L),
    FMT(F32B),
    // - 64bit / 8byte  <- float
    FMT(F64L),
    FMT(F64B),
    // - 8bit / 1byte   <- Mu-Law
    FMT(MU_LAW),
    // - 8bit / 1byte   <- A-Law
    FMT(A_LAW),
    // - 4bit / 0.5byte <- Ima-ADPCM
    FMT(IMA_ADPCM),
    // - 计数
    FMT(CNT),
    // - 按通道存储 (如果是就给 fmt 加上此值)
    FMT(PLANE) = 32,

#if LITTLE_ENDIAN
    FMT(S16)    = FMT(S16L),
    FMT(U16)    = FMT(U16L),
    FMT(S24)    = FMT(S24L),
    FMT(U24)    = FMT(U24L),
    FMT(S24_32) = FMT(S24L32),
    FMT(U24_32) = FMT(U24L32),
    FMT(S32)    = FMT(S32L),
    FMT(U32)    = FMT(U32L),
    FMT(S64)    = FMT(S64L),
    FMT(U64)    = FMT(U64L),
    FMT(F16)    = FMT(F16L),
    FMT(F32)    = FMT(F32L),
    FMT(F64)    = FMT(F64L),
#elif BIG_ENDIAN
    FMT(S16)    = FMT(S16B),
    FMT(U16)    = FMT(U16B),
    FMT(S24)    = FMT(S24B),
    FMT(U24)    = FMT(U24B),
    FMT(S24_32) = FMT(S24B32),
    FMT(U24_32) = FMT(U24B32),
    FMT(S32)    = FMT(S32B),
    FMT(U32)    = FMT(U32B),
    FMT(S64)    = FMT(S64B),
    FMT(U64)    = FMT(U64B),
    FMT(F16)    = FMT(F16B),
    FMT(F32)    = FMT(F32B),
    FMT(F64)    = FMT(F64B),
#endif
#ifndef __cplusplus
} sound_pcmfmt_t;
#else
};
#endif
#undef FMT

typedef struct vsound *vsound_t;

typedef int (*vsound_open_t)(vsound_t vsound);
typedef void (*vsound_close_t)(vsound_t vsound);
typedef int (*vsound_func_t)(vsound_t vsound);
typedef int (*vsound_setvol_t)(vsound_t vsound, float volume);
typedef ssize_t (*vsound_read_t)(vsound_t vsound, void *addr, size_t len);
typedef ssize_t (*vsound_write_t)(vsound_t vsound, const void *addr, size_t len);
typedef int (*vsound_cb_dma_t)(vsound_t vsound, void *addr);

typedef struct vsound {
    bool            is_registed  : 1; // 是否已注册
    bool            is_output    : 1; // 是否是输出设备
    bool            is_using     : 1; // 是否正在使用
    bool            is_rwmode    : 1; // 读写模式还是共享内存模式
    bool            is_dma_ready : 1; // 是否已经开始 DMA
    bool            is_running   : 1; // 是否正在播放或录音
    const char     *name;             // 设备名
    vsound_open_t   open;             // 打开设备
    vsound_close_t  close;            // 关闭设备
    vsound_func_t   play;             // 开始播放
    vsound_func_t   pause;            // 暂停播放
    vsound_func_t   drop;             // 停止播放并丢弃缓冲
    vsound_func_t   drain;            // 等待播放完毕后停止播放
    vsound_read_t   read;             // 读取数据
    vsound_write_t  write;            // 写入数据
    vsound_cb_dma_t start_dma;        // 缓冲区已填满，开始 DMA
    vsound_setvol_t setvol;           // 设置音量
    uint32_t        supported_fmts;   // 支持的采样格式
    uint32_t        supported_rates;  // 支持的采样率
    uint32_t        supported_chs;    // 支持的声道数
    uint16_t        fmt;              // 采样格式
    uint16_t        channels;         // 声道数
    uint32_t        rate;             // 采样率
    float           volume;           // 音量
    uint16_t        bytes_per_sample; // 每个采样的字节数
    void           *buf;              // 当前使用的缓冲区
    size_t          bufpos;           //
    struct queue    bufs0;            // 空缓冲区的队列
    struct queue    bufs1;            // 已写入缓冲区的队列
    size_t          bufsize;          // 每个缓冲区大小
} *vsound_t;

typedef struct ImaAdpcmCtx {
    int index;
    int prev_sample;
} ImaAdpcmCtx;

void sound_ima_adpcm_encode(ImaAdpcmCtx *ctx, void *dst, const int16_t *src, size_t len);
void sound_ima_adpcm_decode(ImaAdpcmCtx *ctx, int16_t *dst, const void *src, size_t len);

bool sound_fmt_issigned(sound_pcmfmt_t fmt);
bool sound_fmt_isfloat(sound_pcmfmt_t fmt);
bool sound_fmt_isbe(sound_pcmfmt_t fmt);
int  sound_fmt_bytes(sound_pcmfmt_t fmt);

bool     vsound_regist(vsound_t device);
bool     vsound_set_supported_fmt(vsound_t device, int16_t fmt);
bool     vsound_set_supported_rate(vsound_t device, int32_t rate);
bool     vsound_set_supported_ch(vsound_t device, int16_t ch);
bool     vsound_set_supported_fmts(vsound_t device, const int16_t *fmts, ssize_t len);
bool     vsound_set_supported_rates(vsound_t device, const int32_t *rates, ssize_t len);
bool     vsound_set_supported_chs(vsound_t device, const int16_t *chs, ssize_t len);
void     vsound_addbuf(vsound_t device, void *buf);
void     vsound_addbufs(vsound_t device, void *const *bufs, ssize_t len);
vsound_t vsound_find(const char *name);
int      vsound_played(vsound_t snd);
int      vsound_clearbuffer(vsound_t snd);
int      vsound_open(vsound_t snd);
int      vsound_close(vsound_t snd);
int      vsound_play(vsound_t snd);
int      vsound_pause(vsound_t snd);
int      vsound_drop(vsound_t snd);
int      vsound_drain(vsound_t snd);
ssize_t  vsound_read(vsound_t snd, void *data, size_t len);
ssize_t  vsound_write(vsound_t snd, const void *data, size_t len);
float    vsound_getvol(vsound_t snd);
int      vsound_setvol(vsound_t snd, float vol);

void mp3_player(const char *path);
void wav_player(const char *path);
