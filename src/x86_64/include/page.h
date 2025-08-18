#pragma once

#define PTE_PRESENT    (0x1UL << 0)  // 页面是否存在
#define PTE_WRITEABLE  (0x1UL << 1)  // 页面可写
#define PTE_USER       (0x1UL << 2)  // 页面是否可被用户访问
#define PTE_HUGE       (0x1UL << 7)  // 大页标志 (页表项为 PAT位)
#define PTE_NO_EXECUTE (0x1UL << 63) // 不可执行
#define PTE_DIS_CACHE  (1ULL << 4)   // 禁用缓存
#define PTE_PWT        (1ULL << 3)   // CPU缓存写通策略
#define PTE_U_ACCESSED (1ULL << 5)   // 已访问 (CPU主动标记)
#define PTE_U_DIRTY    (1ULL << 6)   // 已写入 (CPU主动标记)

#define MAP_ANONYMOUS 32
#define MAP_FIXED     16

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

#define KERNEL_PTE_FLAGS (PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE)

#ifndef PAGE_SIZE
#    define PAGE_SIZE ((size_t)4096)
#endif

#define PAGE_MASK  (~(PAGE_SIZE - 1))
#define ENTRY_MASK 0x1FF

#include "ctype.h"

typedef struct page_table_entry {
    uint64_t value;
} __attribute__((packed)) page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} __attribute__((packed)) page_table_t;

typedef struct page_directory {
    page_table_t *table;
} page_directory_t;

// 懒分配
typedef struct mm_virtual_page {
    uint64_t start;
    uint64_t count;
    uint64_t flags;
    uint64_t pte_flags;
    size_t   index;
} mm_virtual_page_t;

/**
 * 获取内核用页表
 * @return 页表指针
 */
page_directory_t *get_kernel_pagedir();

/**
 * 映射一页地址到指定物理地址
 * @param directory 页表
 * @param addr 虚拟地址
 * @param frame 物理地址
 * @param flags 页表项标志位
 */
void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);

/**
 * 映射一页地址到指定物理地址 (1G页)
 * @param directory 页表
 * @param addr 虚拟地址
 * @param frame 物理地址 (必须用 alloc_frames_1G 分配)
 * @param flags 页表项标志位
 */
void page_map_to_1G(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);

/*
 * 映射一页地址到指定物理地址 (2M页)
 * @param directory 页表
 * @param addr 虚拟地址
 * @param frame 物理地址 (必须用 alloc_frames_2M 分配)
 * @param flags 页表项标志位
 */
void page_map_to_2M(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);

enum PagingMode {
    P1G = 3,
    P2M = 2,
    P4K = 1
};
void page_map_ranges(page_directory_t* directory, uint64_t virtual_address, uint64_t physical_address,uint64_t page_count, uint64_t flags, PagingMode mode);

/**
 * 映射一组物理地址 (对应的虚拟地址用hhdm计算)
 * @param directory 页表
 * @param frame 物理地址
 * @param length 长度
 * @param flags 页表项标志位
 */
void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags);

/**
 * 映射一组物理地址并指定物理地址基址
 * @param directory 页表
 * @param addr 虚拟地址
 * @param frame 物理地址
 * @param length 长度
 * @param flags 页表项标志位
 */
void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags);

/**
 * 将指定虚拟地址随机映射到物理地址上(物理地址由页框分配器决定)
 * @param directory 页表
 * @param addr 虚拟地址
 * @param length 长度
 * @param flags 页表项标志位
 */
void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags);

/**
 * 克隆指定页表
 * @param dir 源页表
 * @param all_copy 是否深拷贝 (false不会拷贝内核部分)
 * @return == NULL ? 未分配成功 : 新页表
 */
page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy);

/**
 * 释放指定页表 (不得为内核页, 内核页由引导程序提供,不遵循页框分配器的规则)
 * @param dir 待释放页表
 */
void free_page_directory(page_directory_t *dir);

/**
 * 多核页切换(不会切换进程的页表, 一般用于进程上下文切换)
 * @param dir 目标页表
 */
void switch_page_directory(page_directory_t *dir);

/**
 * 进程页切换(一般用于内核主动性页表切换)
 * @param dir 目标页表
 */
void switch_process_page_directory(page_directory_t *dir);

/**
 * 分配一页大小的内存
 * @param directory 页目录
 * @param length 长度(字节为单位)
 * @param flags 页标志
 * @return == -1 ? 未分配成功 : 地址
 */
uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags);

/**
 * 释放一段页映射 (未使用 alloc_frames 的页不可使用此方法取消映射)
 * @param directory 页表
 * @param vaddr 虚拟地址 (4k对齐)
 * @param size 大小
 */
void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);

/**
 * 释放指定地址的映射 (未使用 alloc_frames 的页不可使用此方法取消映射)
 * @param directory 页表
 * @param vaddr 虚拟地址 (4k对齐)
 */
void unmap_page(page_directory_t *directory, uint64_t vaddr);

/**
 * 释放指定地址的映射 (未使用 alloc_frames_2M 的页不可使用此方法取消映射)
 * @param directory 页表
 * @param vaddr 虚拟地址 (2M对齐)
 */
void unmap_page_2M(page_directory_t *directory, uint64_t vaddr);

/**
 * 释放指定地址的映射 (未使用 alloc_frames_1G 的页不可使用此方法取消映射)
 * @param directory 页表
 * @param vaddr 虚拟地址 (1G对齐)
 */
void unmap_page_1G(page_directory_t *directory, uint64_t vaddr);

/**
 * 获取指定地址的页表项标志
 * @param directory 页表
 * @param addr 地址 (需要 4k 对齐)
 * @param out_flags 输出的标志
 * @return 是否成功获取
 */
bool page_table_get_flags(page_directory_t *directory, uint64_t addr, uint64_t *out_flags);

/**
 * 设置指定地址的页表项标志
 * @param root 页表
 * @param addr 地址 (需要 4k 对齐)
 * @param new_flags 标志
 * @return 是否成功设置
 */
bool page_table_update_flags(page_directory_t *directory, uint64_t addr, uint64_t new_flags);

/**
 * 获取当前CPU核心的页表
 * @return 页表指针
 */
page_directory_t *get_current_directory();

/*
 * 将引导器提供的页表置换成 CP_Kernel 自己构建的页表
 * 为解决引导器页表占用内存未被标记容易被覆写问题
 */
void switch_cp_kernel_page_directory();
void page_setup();
