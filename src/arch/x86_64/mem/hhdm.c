#include "hhdm.h"
#include "boot.h"
#include "io.h"
#include "krlibc.h"
#include "page.h"

uint64_t physical_memory_offset = 0;

void init_hhdm() {
    physical_memory_offset = boot_physical_memory_offset();
}

uint64_t get_physical_memory_offset() {
    return physical_memory_offset;
}

void *phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0) return NULL;
    return (void *)(phys_addr + physical_memory_offset);
}

void *virt_to_phys(uint64_t virt_addr) {
    if (virt_addr == 0) return NULL;
    return (void *)(virt_addr - physical_memory_offset);
}

void *driver_phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0) return NULL;
    return (void *)(phys_addr + DRIVER_AREA_MEM);
}

void *driver_virt_to_phys(uint64_t virt_addr) {
    if (virt_addr == 0) return NULL;
    return (void *)(virt_addr - DRIVER_AREA_MEM);
}

uint64_t page_virt_to_phys(uint64_t va) {
    uint64_t  pml4_phys = get_cr3();
    uint64_t *pml4      = (uint64_t *)phys_to_virt(pml4_phys);

    size_t pml4_idx = (va >> 39) & ENTRY_MASK;
    size_t pdpt_idx = (va >> 30) & ENTRY_MASK;
    size_t pd_idx   = (va >> 21) & ENTRY_MASK;
    size_t pt_idx   = (va >> 12) & ENTRY_MASK;
    size_t offset   = va & 0xFFF;

    uint64_t pml4e = pml4[pml4_idx];
    if (!(pml4e & PTE_PRESENT)) return 0; // not mapped
    uint64_t *pdpt = (uint64_t *)phys_to_virt(pml4e & PAGE_MASK);

    uint64_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & PTE_PRESENT)) return 0;
    if (pdpte & PTE_HUGE) { return (pdpte & ~((1ULL << 30) - 1)) + (va & ((1ULL << 30) - 1)); }
    uint64_t *pd = (uint64_t *)phys_to_virt(pdpte & PAGE_MASK);

    uint64_t pde = pd[pd_idx];
    if (!(pde & PTE_PRESENT)) return 0;
    if (pde & PTE_HUGE) { return (pde & ~((1ULL << 21) - 1)) + (va & ((1ULL << 21) - 1)); }
    uint64_t *pt = (uint64_t *)phys_to_virt(pde & PAGE_MASK);

    uint64_t pte = pt[pt_idx];
    if (!(pte & PTE_PRESENT)) return 0;

    uint64_t pa = (pte & PAGE_MASK) + offset;
    return pa;
}
