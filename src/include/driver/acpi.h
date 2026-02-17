#pragma once

#include "boot.h"
#include "types.h"
#include "lib/acpica/acpi.h"

void acpi_init();
void acpi_namespace_setup();

typedef struct {
    ACPI_TABLE_HEADER *hdr;
    UINT32 instance;
    char signature[5];
} acpi_table_handle_t;

ACPI_STATUS acpi_table_find_by_signature(const char *signature, acpi_table_handle_t *out_table);
ACPI_STATUS acpi_table_find_next_with_same_signature(acpi_table_handle_t *in_out_table);
void acpi_table_put(acpi_table_handle_t *table);
