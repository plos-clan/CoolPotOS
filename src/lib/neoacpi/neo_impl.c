#include "term/klog.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "mem/frame.h"

#include "lib/neoacpi/neo_impl.h"

void *neo_acpi_kernel_map(neo_acpi_phys_addr addr, size_t len) {
    void *vaddr = (void *)phys_to_virt(addr);
    page_map_range(get_kernel_pagedir(), (uint64_t)vaddr & ~(PAGE_SIZE - 1),
                   addr & ~(PAGE_SIZE - 1), len, KERNEL_PTE_FLAGS);
    return vaddr;
}

void neo_acpi_kernel_unmap(void *addr, size_t len) {
    unmap_page_range(get_kernel_pagedir(), (uint64_t)addr & ~(PAGE_SIZE - 1), len);
}

void *neo_acpi_malloc(size_t size) {
    return malloc(size);
}

void neo_acpi_free(void *ptr) {
    free(ptr);
}

void neo_acpi_info_logger(char *buffer) {
    printk("%s",buffer);
}

uintptr_t neo_acpi_kernel_io_map(neo_acpi_phys_addr base, size_t len) {
    return base;
}

void neo_acpi_kernel_io_unmap(neo_acpi_phys_addr handle) {}

uint8_t neo_acpi_kernel_io_read8(uintptr_t base, size_t offset) {
    //TODO return io_in8((uint64_t)base + offset);
    return 0;
}

uint16_t neo_acpi_kernel_io_read16(uintptr_t base, size_t offset) {
    //TODO return io_in16((uint64_t)base + offset);
    return 0;
}

uint32_t neo_acpi_kernel_io_read32(uintptr_t base, size_t offset) {
    //TODO return io_in32((uint64_t)base + offset);
    return 0;
}

void neo_acpi_kernel_io_write8(uintptr_t base, size_t offset, uint8_t in_value) {
    //TODO io_out8((uint64_t)base + offset, in_value);
}

void neo_acpi_kernel_io_write16(uintptr_t base, size_t offset, uint16_t in_value) {
    //TODO io_out16((uint64_t)base + offset, in_value);
}

void neo_acpi_kernel_io_write32(uintptr_t base, size_t offset, uint32_t in_value) {
    //TODO io_out32((uint64_t)base + offset, in_value);
}
