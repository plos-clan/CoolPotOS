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

#define PAGE_TABLE_FLAG_READ        (1UL << 1) // 页存在/可读
#define PAGE_TABLE_FLAG_WRITE       (1UL << 2) // 页可写
#define PAGE_TABLE_FLAG_EXEC        (1UL << 3) // 页可执行
#define PAGE_TABLE_FLAG_COW         (1UL << 4) // 写时复制
#define PAGE_TABLE_FLAG_UNCACHEABLE (1UL << 5) // 禁用缓存
#define PAGE_TABLE_FLAG_USER        (1UL << 6) // 用户态

#include "types.h"

#if defined(__x86_64__) || defined(__amd64__)
#    include "page_x64.h"
#elif defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    include "page_rv64.h"
#elif defined(__loongarch__) || defined(__loongarch64)
#    include "page_la64.h"
#endif

#define PAGE_CALC_PAGE_TABLE_SIZE(level)                                                           \
    ((uint64_t)1                                                                                   \
     << (ARCH_PT_OFFSET_BASE + (arch_page_table_levels() - (level)) * ARCH_PT_OFFSET_PER_LEVEL))
#define PAGE_CALC_PAGE_TABLE_MASK(level) (PAGE_CALC_PAGE_TABLE_SIZE(level) - (uint64_t)1)
#define PAGE_CALC_PAGE_TABLE_INDEX(vaddr, level)                                                   \
    (((vaddr)                                                                                      \
      >> (ARCH_PT_OFFSET_BASE + (arch_page_table_levels() - (level)) * ARCH_PT_OFFSET_PER_LEVEL))  \
     & (((uint64_t)1 << ARCH_PT_OFFSET_PER_LEVEL) - 1))

typedef uint64_t *page_directory_t;

uint64_t arch_transform_pt_flags(uint64_t flags);

uint64_t arch_page_table_levels();
void arch_page_table_init();

void page_map_range(
    page_directory_t directory, uint64_t addr, uint64_t frame, uint64_t length, uint64_t flags
);

void page_map_range_to_random(
    page_directory_t directory, uint64_t addr, uint64_t length, uint64_t flags
);

bool page_map_to(
    page_directory_t directory, uint64_t addr, uint64_t frame, uint64_t arch_flags, bool force
);
uint64_t arch_make_page_table_entry(uint64_t paddr, uint64_t flags);
void arch_flush_tlb(uint64_t vaddr);
void arch_flush_tlb_all();

page_directory_t get_kernel_page_dir();
void set_kernel_dir(page_directory_t directory);

bool copy_to_user(void *dst, const void *src, size_t size);
bool copy_from_user(void *dst, const void *src, size_t size);

page_directory_t get_current_page_dir(bool user);

void page_init();
