#ifndef CRASHPOWEROS_MEMORY_H
#define CRASHPOWEROS_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "isr.h"

#define PAGE_SIZE 4096
#define KHEAP_INITIAL_SIZE 0xf00000
#define STACK_SIZE 32768

#define USER_EXEC_FILE_START 0xa0000000
#define USER_START 0xb0000000
#define USER_END (USER_START + 0x2000000)
#define USER_HEAP_END (USER_END - STACK_SIZE)

#define INDEX_FROM_BIT(a) (a / (8*4))
#define OFFSET_FROM_BIT(a) (a % (8*4))

typedef char ALIGN[16];

#include "multiboot.h"
#include "common.h"

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
}__attribute__((packaged)) page_t;

typedef struct page_table {
    page_t pages[1024];
}__attribute__((packaged)) page_table_t;

typedef struct page_directory {
    page_table_t volatile*tables[1024];
    uint32_t table_phy[1024];
    uint32_t physicalAddr;
}__attribute__((packaged)) page_directory_t;

typedef union header {
    struct {
        uint32_t size;
        uint32_t is_free;
        union header *next;
    } s;
    ALIGN stub;
} header_t;

uint32_t memory_usage();

void* memcpy(void* s, const void* ct, size_t n);

int memcmp(const void *a_, const void *b_, uint32_t size);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t num);

void switch_page_directory(page_directory_t *dir); //页切换

page_t *get_page(uint32_t address, int make, page_directory_t *dir); //获取指定地址的页映射 (make为1造页 ist决定是否开启DEBUG输出)

void alloc_frame(page_t *page, int is_kernel, int is_writable); //为一个页分配一个物理页框

uint32_t first_frame(); //获取第一个空闲页框

void page_fault(registers_t *regs);

page_directory_t *clone_directory(page_directory_t *src); //克隆页表

void kfree(void *ptr);
void *krealloc(void *cp, size_t nbytes);
void *kmalloc(size_t nbytes);
void *kcalloc(size_t nelem, size_t elsize);
size_t kmalloc_usable_size(void *cp);

void init_page(multiboot_t *mboot);

void memclean(char *s, int len);

void alloc_frame_line(page_t *page, uint32_t line,int is_kernel, int is_writable); //映射指定物理地址的页框

void free_frame(page_t *page); //释放页框

void page_flush(page_directory_t *dir);

#endif //CRASHPOWEROS_MEMORY_H
