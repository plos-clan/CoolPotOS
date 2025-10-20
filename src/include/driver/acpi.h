#pragma once

#include "types.h"

void acpi_init();
void acpi_namespace_setup();
uintptr_t boot_get_acpi_rsdp();
