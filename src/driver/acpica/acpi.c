#include "driver/acpi.h"
#include "krlibc.h"
#include "lib/acpica/acpi.h"
#include "term/klog.h"

void acpi_init() {
    ACPI_STATUS st;
    st = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(st))
        goto error;
    st = AcpiInitializeTables(NULL, 16, TRUE);
    if (ACPI_FAILURE(st))
        goto error;
    return;
error:
    kerror("acpica initialize failure");
    while (true)
        arch_wait_for_interrupt();
}

void acpi_namespace_setup() {
    ACPI_STATUS st;
    st = AcpiLoadTables();
    if (ACPI_FAILURE(st))
        goto error;
    st = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(st))
        goto error;
    st = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(st))
        goto error;
    return;
error:
    kerror("acpica namespace setup fault");
}

ACPI_STATUS acpi_table_find_by_signature(const char *signature, acpi_table_handle_t *out_table) {
    size_t      i;
    ACPI_STATUS st;

    if (!signature || !out_table) {
        return AE_BAD_PARAMETER;
    }

    memset(out_table, 0, sizeof(*out_table));
    for (i = 0; i < 4 && signature[i]; i++) {
        out_table->signature[i] = signature[i];
    }
    if (i < 4) {
        return AE_BAD_PARAMETER;
    }
    out_table->signature[4] = '\0';
    out_table->instance     = 1;

    st = AcpiGetTable(out_table->signature, out_table->instance, &out_table->hdr);
    if (ACPI_FAILURE(st)) {
        out_table->hdr = NULL;
    }
    return st;
}

ACPI_STATUS acpi_table_find_next_with_same_signature(acpi_table_handle_t *in_out_table) {
    ACPI_STATUS st;

    if (!in_out_table || !in_out_table->signature[0]) {
        return AE_BAD_PARAMETER;
    }

    if (in_out_table->hdr) {
        AcpiPutTable(in_out_table->hdr);
        in_out_table->hdr = NULL;
    }

    in_out_table->instance++;
    st = AcpiGetTable(in_out_table->signature, in_out_table->instance, &in_out_table->hdr);
    if (ACPI_FAILURE(st)) {
        in_out_table->hdr = NULL;
    }
    return st;
}

void acpi_table_put(acpi_table_handle_t *table) {
    if (!table || !table->hdr) {
        return;
    }
    AcpiPutTable(table->hdr);
    table->hdr = NULL;
}
