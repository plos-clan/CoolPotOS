#include "boot.h"
#include "krlibc.h"

#define PAGE_SIZE 4096
#define PTE_PER_TABLE 512

// SV48 页表项标志位
#define PTE_V (1UL << 0) // Valid
#define PTE_R (1UL << 1) // Readable
#define PTE_W (1UL << 2) // Writable
#define PTE_X (1UL << 3) // Executable
#define PTE_U (1UL << 4) // User
#define PTE_G (1UL << 5) // Global
#define PTE_A (1UL << 6) // Accessed
#define PTE_D (1UL << 7) // Dirty

typedef uint64_t pte_t;

// 页面大小常量
#define SIZE_4K (1UL << 12)
#define SIZE_2M (1UL << 21)
#define SIZE_1G (1UL << 30)
#define SIZE_512G (1UL << 39)

// 获取虚拟地址在各级页表中的索引
#define VPN(va, level) (((va) >> (12 + 9 * (level))) & 0x1FF)

// 物理地址和PTE之间的转换
#define PA_TO_PTE(pa) (((pa) >> 12) << 10)
#define PTE_TO_PA(pte) (((pte) >> 10) << 12)

// 高半核地址偏移
#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000UL

// 内核预留的页表空间（在数据段中）
#define PAGE_TABLE_POOL_SIZE (4 * 1024 * 1024) // 4MB 用于页表
static uint8_t page_table_pool[PAGE_TABLE_POOL_SIZE]
    __attribute__((aligned(PAGE_SIZE)));
static size_t page_table_pool_used = 0;

// 辅助宏
#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#endif

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#endif

static uintptr_t alloc_page_table() {
    if (page_table_pool_used + PAGE_SIZE > PAGE_TABLE_POOL_SIZE) {
        return 0; // 池耗尽
    }

    uintptr_t page = (uintptr_t)page_table_pool + page_table_pool_used;

    // 清零页表
    memset((void *)page, 0, PAGE_SIZE);

    page_table_pool_used += PAGE_SIZE;
    return page;
}

static pte_t *walk_page_table(pte_t *table, uintptr_t va, int level,
                              int huge_level, int alloc) {
    size_t idx = VPN(va, level);
    pte_t *pte = &table[idx];

    if (level == huge_level) {
        return pte; // 返回叶级PTE
    }

    pte_t *next_table;
    if (*pte & PTE_V) {
        // 页表已存在
        next_table = (pte_t *)PTE_TO_PA(*pte);
    } else {
        if (!alloc) {
            return NULL;
        }
        // 需要创建下一级页表
        uintptr_t new_table = alloc_page_table();
        if (!new_table) {
            return NULL;
        }
        *pte = PA_TO_PTE(new_table - KERNEL_VIRTUAL_BASE) | PTE_V;
        next_table = (pte_t *)new_table;
    }

    return walk_page_table(next_table, va, level - 1, huge_level, alloc);
}

static int map_4k_page(pte_t *root_table, uintptr_t va, uintptr_t pa,
                       uint64_t perm) {
    pte_t *pte = walk_page_table(root_table, va, 3, 0, 1);
    if (!pte) {
        return -1;
    }

    // 检查是否已映射
    if (*pte & PTE_V) {
        return 0; // 已经映射
    }

    *pte = PA_TO_PTE(pa) | perm | PTE_V;
    return 0;
}

static int map_2m_page(pte_t *root_table, uintptr_t va, uintptr_t pa,
                       uint64_t perm) {
    pte_t *pte = walk_page_table(root_table, va, 3, 1, 1);
    if (!pte) {
        return -1;
    }

    // 在L1级别创建2MB大页
    *pte = PA_TO_PTE(pa) | perm | PTE_V;
    return 0;
}

static int map_1g_page(pte_t *root_table, uintptr_t va, uintptr_t pa,
                       uint64_t perm) {
    pte_t *pte = walk_page_table(root_table, va, 3, 2, 1);
    if (!pte) {
        return -1;
    }

    // 在L2级别创建1GB大页
    *pte = PA_TO_PTE(pa) | perm | PTE_V;
    return 0;
}

static int map_region(pte_t *root_table, uintptr_t pa_start, uintptr_t pa_end,
                      uint64_t perm) {
    uintptr_t current = ALIGN_UP(pa_start, SIZE_4K);
    uintptr_t end = ALIGN_DOWN(pa_end, SIZE_4K);

    if (current >= end) {
        return 0;
    }

    while (current < end) {
        uintptr_t remaining = end - current;

        // 优先尝试1GB大页
        if (ALIGN_UP(current, SIZE_1G) == current && remaining >= SIZE_1G) {
            if (map_1g_page(root_table, current + KERNEL_VIRTUAL_BASE, current,
                            perm) < 0) {
                return -1;
            }
            current += SIZE_1G;
            continue;
        }

        // 然后尝试2MB大页
        if (ALIGN_UP(current, SIZE_2M) == current && remaining >= SIZE_2M) {
            if (map_2m_page(root_table, current + KERNEL_VIRTUAL_BASE, current,
                            perm) < 0) {
                return -1;
            }
            current += SIZE_2M;
            continue;
        }

        // 最后使用4KB页
        if (map_4k_page(root_table, current + KERNEL_VIRTUAL_BASE, current,
                        perm) < 0) {
            return -1;
        }
        current += SIZE_4K;
    }

    return 0;
}

extern void *opensbi_dtb_vaddr;

int setup_sv48_page_table(boot_memory_map_t *mmap, uint64_t *satp_out) {
    // 重置页表池
    page_table_pool_used = 0;

    // 分配根页表（L3）
    uintptr_t root_table = alloc_page_table();
    if (!root_table) {
        return -1;
    }

    // 首先映射内核已经占用的区域（确保内核代码可继续执行）
    uintptr_t kernel_low_start = 0x80000000UL;
    uintptr_t kernel_low_end = 0x81000000UL; // 16MB内核区域

    if (map_region((pte_t *)root_table, kernel_low_start, kernel_low_end,
                   PTE_R | PTE_W | PTE_X | PTE_G | PTE_A | PTE_D) < 0) {
        return -1;
    }

    uintptr_t dtb_paddr = (uintptr_t)opensbi_dtb_vaddr - KERNEL_VIRTUAL_BASE;

    if (map_region((pte_t *)root_table, dtb_paddr, dtb_paddr + SIZE_2M,
                   PTE_R | PTE_W | PTE_X | PTE_G | PTE_A | PTE_D) < 0) {
        return -1;
    }

    for (size_t i = 0; i < mmap->entry_count; i++) {
        if (mmap->entries[i].type == BOOT_MMAP_USABLE && mmap->entries[i].length > 0) {
            uintptr_t start = mmap->entries[i].base;
            uintptr_t end = start + mmap->entries[i].length;

            // 只映射前4G内的区域
            uintptr_t map_end = end;

            // 跳过内核已经映射的区域
            if (start < kernel_low_end && map_end > kernel_low_start) {
                if (start < kernel_low_start) {
                    // 映射内核前的部分
                    if (map_region((pte_t *)root_table, start, kernel_low_start,
                                   PTE_R | PTE_W | PTE_G | PTE_A | PTE_D) < 0) {
                        return -1;
                    }
                }
                if (map_end > kernel_low_end) {
                    // 映射内核后的部分
                    if (map_region((pte_t *)root_table, kernel_low_end, map_end,
                                   PTE_R | PTE_W | PTE_G | PTE_A | PTE_D) < 0) {
                        return -1;
                    }
                }
            } else {
                // 完全在内核区域之外
                if (map_region((pte_t *)root_table, start, map_end,
                               PTE_R | PTE_W | PTE_G | PTE_A | PTE_D) < 0) {
                    return -1;
                }
            }
        }
    }

    // 构造 SATP 寄存器的值
    // SATP format for SV48: [63:60]=MODE(9), [59:44]=ASID(0), [43:0]=PPN
    uint64_t ppn = (root_table - KERNEL_VIRTUAL_BASE) >> 12;
    *satp_out = (9UL << 60) | ppn;

    return 0;
}

static inline void enable_paging(uint64_t satp) {
    __asm__ volatile("csrw satp, %0\n"
                     "sfence.vma zero, zero\n"
                     :
                     : "r"(satp)
                     : "memory");
}

extern boot_memory_map_t opensbi_memory_map;

void init_early_paging() {
    uint64_t satp;

    if (setup_sv48_page_table(&opensbi_memory_map, &satp) < 0) {
        // 初始化失败，保持在无分页模式
        return;
    }

    // 启用分页
    enable_paging(satp);
}