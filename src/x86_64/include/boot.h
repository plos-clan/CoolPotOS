#pragma once

#include "ctype.h"
#include "limine.h"

const char                    *get_bootloader_name();
const char                    *get_bootloader_version();
uint64_t                       boot_physical_memory_offset();
uint64_t                       get_memory_size();
struct limine_memmap_response *get_memory_map();
bool                           x2apic_mode_supported();
struct limine_smp_response    *get_smp_info();
void                          *get_acpi_rsdp();
struct limine_framebuffer     *get_graphics_framebuffer();
