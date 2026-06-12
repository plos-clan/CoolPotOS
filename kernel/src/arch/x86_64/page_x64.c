#include "io.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "cpu_local.h"

static bool page_table_levels_valid(uint64_t levels) {
    return levels > 0 && levels <= ARCH_MAX_PT_LEVEL;
}

uint64_t arch_transform_pt_flags(uint64_t flags) {
    uint64_t pt_flags = 0;

    if ((flags & PAGE_TABLE_FLAG_EXEC) == 0) {
        pt_flags |= ARCH_PT_FLAG_NX;
    }
    if ((flags & PAGE_TABLE_FLAG_WRITE) != 0) {
        pt_flags |= ARCH_PT_FLAG_WRITEABLE;
    }
    if ((flags & PAGE_TABLE_FLAG_USER) != 0) {
        pt_flags |= ARCH_PT_FLAG_USER;
    }
    if ((flags & PAGE_TABLE_FLAG_UNCACHEABLE) != 0) {
        pt_flags |= ARCH_PT_FLAG_PCD | ARCH_PT_FLAG_PWT;
    }

    return pt_flags;
}

uint64_t arch_page_table_levels() {
    return 4;
}

void arch_set_current_directory(page_directory_t *dir) {
    uint64_t current_directory = (uint64_t)dir;
    __asm__ volatile("movq %0, %%cr3" ::"r"(current_directory) : "memory");
}

void arch_flush_tlb(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" ::"r"(PADDING_DOWN(addr, PAGE_SIZE)) : "memory");
}

void arch_flush_tlb_all() {
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3)::"memory");
    __asm__ volatile("movq %0, %%cr3" ::"r"(cr3) : "memory");
}

uint64_t arch_make_page_table_entry(uint64_t paddr, uint64_t flags) {
    return ARCH_MAKE_PDE(paddr, ARCH_PT_TABLE_FLAGS | (flags & ARCH_PT_FLAG_USER));
}

void arch_page_table_init() {
    page_directory_t kernel_page_table = phys_to_virt(get_cr3());
    set_kernel_dir(kernel_page_table);
}

static uint64_t user_translate_access(uint64_t *pgdir, uint64_t uaddr, bool write) {
    if (!pgdir || !uaddr)
        return 0;

    uint64_t levels = arch_page_table_levels();
    if (!page_table_levels_valid(levels))
        return 0;
    uint64_t indexs[ARCH_MAX_PT_LEVEL];
    for (uint64_t i = 0; i < levels; i++) {
        indexs[i] = PAGE_CALC_PAGE_TABLE_INDEX(uaddr, i + 1);
    }

    for (uint64_t i = 0; i < levels - 1; i++) {
        uint64_t entry = pgdir[indexs[i]];
        if (ARCH_PT_IS_LARGE(entry)) {
            uint64_t flags = ARCH_READ_PTE_FLAG(entry);
            if (!(flags & ARCH_PT_FLAG_USER))
                return 0;
            if (write && !(flags & ARCH_PT_FLAG_WRITEABLE))
                return 0;
            return (ARCH_READ_PTE(entry) & ~PAGE_CALC_PAGE_TABLE_MASK(i + 1))
                   + (uaddr & PAGE_CALC_PAGE_TABLE_MASK(i + 1));
        }
        if (!ARCH_PT_IS_TABLE(entry))
            return 0;
        pgdir = (uint64_t *)phys_to_virt(ARCH_READ_PTE(entry));
    }

    uint64_t pte = pgdir[indexs[levels - 1]];
    if (!(pte & ARCH_PT_FLAG_PRESENT))
        return 0;

    uint64_t flags = ARCH_READ_PTE_FLAG(pte);
    if (!(flags & ARCH_PT_FLAG_USER))
        return 0;
    if (write && !(flags & ARCH_PT_FLAG_WRITEABLE))
        return 0;
    return ARCH_READ_PTE(pte) + (uaddr & PAGE_CALC_PAGE_TABLE_MASK(levels));
}

uint64_t user_translate_or_fault(uint64_t *pgdir, uint64_t uaddr, bool write) {
    uint64_t pa = user_translate_access(pgdir, uaddr, write);
    if (pa)
        return pa;

    // task_t *task = arch_get_current();
    // if (!task)
    //     return 0;
    // if (handle_page_fault_flags(task, uaddr, write ? PF_ACCESS_WRITE : PF_ACCESS_READ) != 0)
    //     return 0;

    return user_translate_access(pgdir, uaddr, write);
}

uint64_t user_translate_no_fault(uint64_t *pgdir, uint64_t uaddr, bool write) {
    return user_translate_access(pgdir, uaddr, write);
}

bool copy_to_user(void *dst, const void *src, size_t size) {
    if (size == 0)
        return false;
    if (!src)
        return true;

    if (check_user_overflow((uint64_t)dst, size))
        return true;

    uint64_t *pgdir   = get_current_page_dir(true);
    uint64_t uaddr    = (uint64_t)dst;
    const uint8_t *in = src;
    size_t remain     = size;

    while (remain > 0) {
        uint64_t pa = user_translate_or_fault(pgdir, uaddr, true);
        if (!pa)
            return true;

        size_t chunk = MIN(remain, PAGE_SIZE - (uaddr & (PAGE_SIZE - 1)));
        memcpy(phys_to_virt(pa), in, chunk);

        uaddr += chunk;
        in += chunk;
        remain -= chunk;
    }

    return false;
}

bool copy_from_user(void *dst, const void *src, size_t size) {
    if (size == 0)
        return false;
    if (!dst)
        return true;

    if (check_user_overflow((uint64_t)src, size))
        return true;

    uint64_t *pgdir = get_current_page_dir(true);
    uint64_t uaddr  = (uint64_t)src;
    uint8_t *out    = (uint8_t *)dst;
    size_t remain   = size;

    while (remain > 0) {
        uint64_t pa = user_translate_or_fault(pgdir, uaddr, false);
        if (!pa)
            return true;

        size_t chunk = MIN(remain, PAGE_SIZE - (uaddr & (PAGE_SIZE - 1)));
        memcpy(out, phys_to_virt(pa), chunk);

        uaddr += chunk;
        out += chunk;
        remain -= chunk;
    }

    return false;
}

page_directory_t get_current_page_dir(bool user) {
    (int)user;
    const cpu_local_t *local = get_current_cpu();
    return local->current_dir;
}
