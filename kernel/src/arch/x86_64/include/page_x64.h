#pragma once

#define PAGE_SIZE 4096

#define ARCH_MAX_PT_LEVEL 4

#define ARCH_PT_FLAG_PRESENT   (0x1UL << 0)
#define ARCH_PT_FLAG_WRITEABLE (0x1UL << 1)
#define ARCH_PT_FLAG_USER      (0x1UL << 2)
#define ARCH_PT_FLAG_PWT       (0x1UL << 3)
#define ARCH_PT_FLAG_PCD       (0x1UL << 4)
#define ARCH_PT_FLAG_HUGE      (0x1UL << 7)
#define ARCH_PT_FLAG_COW       (0x1UL << 56)
#define ARCH_PT_FLAG_NX        (0x1UL << 63)
#define ARCH_ADDR_MASK         0x00007FFFFFFFF000

#define ARCH_PT_OFFSET_BASE      12
#define ARCH_PT_OFFSET_PER_LEVEL 9

#define ARCH_ADDR_MASK 0x00007FFFFFFFF000

#define ARCH_PT_TABLE_FLAGS (ARCH_PT_FLAG_PRESENT | ARCH_PT_FLAG_WRITEABLE)

#define ARCH_READ_PTE(pte)          ((uint64_t)(pte) & ARCH_ADDR_MASK)
#define ARCH_MAKE_PTE(paddr, flags) (((uint64_t)(paddr) & ARCH_ADDR_MASK) | (flags))
#define ARCH_MAKE_PDE(paddr, flags) ARCH_MAKE_PTE((paddr), (flags))
#define ARCH_READ_PTE_FLAG(pte)     ((uint64_t)(pte) & ~ARCH_ADDR_MASK)

#define ARCH_PT_IS_TABLE(x) (((x) & ARCH_PT_TABLE_FLAGS) == ARCH_PT_TABLE_FLAGS)
#define ARCH_PT_IS_LARGE(x)                                                                        \
    (((x) & (ARCH_PT_FLAG_PRESENT | ARCH_PT_FLAG_HUGE))                                            \
     == (ARCH_PT_FLAG_PRESENT | ARCH_PT_FLAG_HUGE))
