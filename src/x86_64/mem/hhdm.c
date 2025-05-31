#include "hhdm.h"
#include "boot.h"

uint64_t physical_memory_offset = 0;

void init_hhdm() {
    physical_memory_offset = boot_physical_memory_offset();
}

uint64_t get_physical_memory_offset() {
    return physical_memory_offset;
}

void *phys_to_virt(uint64_t phys_addr) {
    return (void *)(phys_addr + physical_memory_offset);
}

void *virt_to_phys(uint64_t virt_addr) {
    return (void *)(virt_addr - physical_memory_offset);
}
