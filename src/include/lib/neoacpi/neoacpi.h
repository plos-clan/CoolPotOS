#pragma once

#define CONFIG_ACPI_PLAIN_LOG_BUFFER_SIZE 128
#define CONFIG_ACPI_VERSION               "0.0.1"

#include "neo_impl.h"
#include "neotable.h"
#include "neotype.h"

typedef struct {
    acpi_object_name   signature;
    neo_acpi_phys_addr phys_addr;
    uint32_t           length;
} neo_acpi_table_entry_t;

typedef struct neo_acpi_handle {
    neo_acpi_table_entry_t **entries;
    size_t                   entries_length;
} neo_acpi_handle_t;

neo_acpi_handle_t *neo_acpi_initialize(neo_acpi_phys_addr rsdt_base_addr);
bool               table_find_by_signature(const neo_acpi_handle_t *handle, const char signature[4],
                                           struct acpi_table *table);
neo_acpi_handle_t *neo_acpi_rsdt_init(neo_acpi_handle_t *handle, neo_acpi_phys_addr roor_table_phy,
                                      size_t entry_size);
