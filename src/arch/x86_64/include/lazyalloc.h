#pragma once

#include "cptype.h"
#include "pcb.h"

typedef struct mm_virtual_page {
    uint64_t start;
    uint64_t count;
    uint64_t flags;
    uint64_t pte_flags;
    size_t   index;
} mm_virtual_page_t;

void *virt_copy(void *ptr); // fork 懒分配信息复制回调函数

/**
 * 尝试映射一块被懒分配器记录的地址
 * @param pcb 请求的进程
 * @param address 地址
 * @return 0 代表成功, 否则分配失败
 */
errno_t lazy_tryalloc(pcb_t pcb, uint64_t address);

/**
 * 解除一个进程的指定懒分配信息地址区间映射关系
 * @param process 释放进程
 * @param vaddr 基址
 * @param length 长度
 */
void unmap_virtual_page(pcb_t process, uint64_t vaddr, size_t length);

/**
 * 进程向懒分配器递交内存
 * @param process 目标进程
 * @param vaddr 虚拟地址
 * @param length 递交长度
 * @param page_flags 页标志
 * @param flags 分配信息标志
 */
void lazy_infoalloc(pcb_t process, uint64_t vaddr, size_t length, uint64_t page_flags,
                    uint64_t flags);

/**
 * 释放指定进程的懒分配信息
 * @param process 释放进程
 */
void lazy_free(pcb_t process);
