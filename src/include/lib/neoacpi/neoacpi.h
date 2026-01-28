#pragma once

#define CONFIG_ACPI_PLAIN_LOG_BUFFER_SIZE 128
#define CONFIG_ACPI_VERSION               "0.0.1"

#include "libaml.h"
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
    aml_context_t           *aml_context;
    struct acpi_fadt         global_fadt;
    bool                     reduced_hardware;
} neo_acpi_handle_t;

neo_acpi_handle_t *neo_acpi_initialize(neo_acpi_phys_addr rsdt_base_addr);
bool               table_find_by_signature(const neo_acpi_handle_t *handle, const char signature[4],
                                           acpi_table *table);
neo_acpi_handle_t *neo_acpi_rsdt_init(neo_acpi_handle_t *handle, neo_acpi_phys_addr roor_table_phy,
                                      size_t entry_size);
neo_acpi_handle_t *acpi_load_fadt(neo_acpi_handle_t *handle);

void aml_context_initialize(neo_acpi_handle_t *handle, struct acpi_dsdt *dsdt);
