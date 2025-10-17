#pragma once

#define MAP_SHARED          0x01
#define MAP_PRIVATE         0x02
#define MAP_SHARED_VALIDATE 0x03
#define MAP_TYPE            0x0f
#define MAP_FIXED           0x10
#define MAP_ANON            0x20
#define MAP_ANONYMOUS       MAP_ANON
#define MAP_NORESERVE       0x4000
#define MAP_GROWSDOWN       0x0100
#define MAP_DENYWRITE       0x0800
#define MAP_EXECUTABLE      0x1000
#define MAP_LOCKED          0x2000
#define MAP_POPULATE        0x8000
#define MAP_NONBLOCK        0x10000
#define MAP_STACK           0x20000
#define MAP_HUGETLB         0x40000
#define MAP_SYNC            0x80000
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FILE            0

#define MREMAP_MAYMOVE   1
#define MREMAP_FIXED     2
#define MREMAP_DONTUNMAP 4

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

#include "types.h"

#if defined(__x86_64__) || defined(__amd64__)
#    include "page_x64.h"
#endif

page_directory_t *get_kernel_pagedir();

void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags);
void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags);

/**
 * 分配一块随机的内核区可用地址
 * @param directory 源页表
 * @param length 分配长度
 * @param flags 分配标志
 */
uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags);

/**
 * 映射一段物理地址不连续的区域
 * @param directory 源页表
 * @param addr 虚拟地址起始
 * @param length 映射长度
 * @param flags 映射标志
 */
void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                                  uint64_t flags);

/**
 * 解除一段地址映射
 * 注意: 该函数会连带释放掉这段地址映射所被分配的物理页框占用
 * @param directory 源页表
 * @param vaddr 虚拟地址起始
 * @param size 大小
 */
void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);

void init_page();
