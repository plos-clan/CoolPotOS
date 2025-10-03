#pragma once

#include "../../../include/ctype.h"

extern uint64_t physical_memory_offset;

void init_hhdm();

/**
 * 将物理地址转换为虚拟地址
 * @param phys_addr 物理地址
 * @return 虚拟地址
 */
void *phys_to_virt(uint64_t phys_addr);

uint64_t get_physical_memory_offset();

/**
 * 将虚拟地址转换成物理地址
 * @param virt_addr 虚拟地址
 * @return 物理地址
 */
void *virt_to_phys(uint64_t virt_addr);

/*
 * 将虚拟地址转换为物理地址 (根据页表解析)
 * @param va 虚拟地址
 * @return 物理地址
 */
uint64_t page_virt_to_phys(uint64_t va);

/**
 * 将物理地址转换为虚拟地址(采用 DRIVER_AREA_MEM 偏移)
 * @param phys_addr 物理地址
 * @return 虚拟地址
 */
void *driver_phys_to_virt(uint64_t phys_addr);

/**
 * 将虚拟地址转换为物理地址(采用 DRIVER_AREA_MEM 偏移)
 * @param virt_addr 虚拟地址
 * @return 物理地址
 */
void *driver_virt_to_phys(uint64_t virt_addr);
