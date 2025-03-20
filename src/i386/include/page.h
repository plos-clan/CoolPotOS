#pragma once

#define PAGE_SIZE 4096

#define INDEX_FROM_BIT(a) (a / (8*4))
#define OFFSET_FROM_BIT(a) (a % (8*4))

#include "multiboot.h"
#include "ctypes.h"

typedef struct page{
    uint8_t present: 1;
    uint8_t rw: 1;
    uint8_t user:1;
    uint8_t pwt:1; // 0回写模式 ; 1 直写模式
    uint8_t pcd:1; // 为1时禁止该页缓冲
    uint8_t accessed: 1;
    uint8_t dirty: 1;
    uint8_t pat: 1;
    uint8_t global: 1;
    uint8_t ignored: 3;
    uint32_t frame: 20;
}__attribute__((packed)) page_t;

typedef struct page_table {
    page_t pages[1024];
}__attribute__((packed)) page_table_t;

typedef struct page_directory {
    page_table_t volatile*tables[1024];
    uint32_t table_phy[1024];
    uint32_t physicalAddr;
}__attribute__((packed)) page_directory_t;

page_directory_t *get_current_directory(); //获取当前页表 (一般是当前进程的页目录)
page_directory_t *clone_directory(page_directory_t *src); //拷贝指定页目录
void alloc_frame_line(page_t *page, uint32_t line,int is_kernel, int is_writable); //映射指定地址的物理页框到指定页
void alloc_frame(page_t *page, int is_kernel, int is_writable); //映射一个页
void free_frame(page_t *page); //释放一个页框
page_t *get_page(uint32_t address, int make, page_directory_t *dir); //获取指定地址的页
void switch_page_directory(page_directory_t *dir); //切换页表
void page_init(multiboot_t *mboot);