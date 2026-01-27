///
/// NeoACPI Kernel API Implement
///
#pragma once

#include "neo_logger.h"
#include "neotype.h"

/**
 * Map a physical address
 *
 * @param addr physical address
 * @param len address size
 * @return virtual address
 */
void *neo_acpi_kernel_map(neo_acpi_phys_addr addr, size_t len);

/**
 * Unmap a virtual address.
 * @param addr virtual address
 * @param len address size
 */
void neo_acpi_kernel_unmap(void *addr, size_t len);

/**
 * Allocate memory of a specified size.
 *
 * @param size memory size
 * @return memory base pointer
 */
void *neo_acpi_malloc(size_t size);

/**
 * Free allocated memory.
 * @param ptr allocated memory
 */
void neo_acpi_free(void *ptr);

/**
 * Print neo acpi log
 * @param buffer log info
 */
void neo_acpi_kernel_logger(logger_level level, char *buffer);

uintptr_t neo_acpi_kernel_io_map(neo_acpi_phys_addr base, size_t len);
void      neo_acpi_kernel_io_unmap(neo_acpi_phys_addr handle);

/**
 * I/O Port or MMIO read interface.
 */
uint8_t  neo_acpi_kernel_io_read8(uint64_t base, size_t offset);
uint8_t  neo_acpi_kernel_io_read8(uintptr_t base, size_t offset);
uint16_t neo_acpi_kernel_io_read16(uintptr_t base, size_t offset);
uint32_t neo_acpi_kernel_io_read32(uintptr_t base, size_t offset);
void     neo_acpi_kernel_io_write8(uintptr_t base, size_t offset, uint8_t in_value);
void     neo_acpi_kernel_io_write16(uintptr_t base, size_t offset, uint16_t in_value);
void     neo_acpi_kernel_io_write32(uintptr_t base, size_t offset, uint32_t in_value);
