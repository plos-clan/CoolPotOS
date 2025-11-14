#pragma once

#include "bitmap.h"
#include "cptype.h"

typedef struct {
    Bitmap bitmap;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

void init_frame();

/**
 * 释放指定地址的一段物理页框
 * @param addr 物理地址
 * @param count 个数
 */
void free_frames(uint64_t addr, size_t count);

/**
 * 释放指定地址的4k物理页框
 * @param addr 物理地址 (4k对齐)
 */
void free_frame(uint64_t addr);

/**
 * 释放指定地址的2M物理页框
 * @param addr 物理地址 (2M对齐)
 */
void free_frames_2M(uint64_t addr);

/**
 * 释放指定地址的1G物理页框
 * @param addr (1G对齐) 物理地址
 */
void free_frames_1G(uint64_t addr);

/**
 * 分配指定数量的物理页框
 * @param count 页框个数
 * @return 分配的物理地址 (4k对齐)
 */
uint64_t alloc_frames(size_t count);

/**
 * 分配指定数量的2M物理页框
 * @param count 页框个数
 * @return 分配的物理地址 (2M对齐)
 */
uint64_t alloc_frames_2M(size_t count);

/**
 * 分配指定数量的1G物理页框
 * @param count 页框个数
 * @return 分配的物理地址 (1G对齐)
 */
uint64_t alloc_frames_1G(size_t count);
