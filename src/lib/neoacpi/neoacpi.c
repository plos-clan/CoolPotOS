/**
 * UinxedKernel & CoolPotOS NeoACPI Library.
 */
#include "lib/neoacpi/neoacpi.h"
#include "lib/neoacpi/neo_impl.h"
#include "lib/neoacpi/neo_logger.h"
#include "lib/neoacpi/neo_stdlib.h"
#include "term/klog.h"

neo_acpi_handle_t *neo_acpi_initialize(neo_acpi_phys_addr rsdt_base_addr) {
    if (rsdt_base_addr == 0) return NULL;

    neo_acpi_phys_addr roor_table_phy = 0;
    size_t             table_entries  = 0;

    neo_acpi_handle_t *handle = neo_acpi_malloc(sizeof(neo_acpi_handle_t));
    if (handle == NULL) {
        log_error("cannot alloc handle memory.");
        return NULL;
    }
    neo_acpi_memset(handle, 0, sizeof(neo_acpi_handle_t));

    log_info("neo acpi version %s", CONFIG_ACPI_VERSION);

    struct acpi_rsdp *rsdp = neo_acpi_kernel_map(rsdt_base_addr, sizeof(struct acpi_rsdp));
    if (rsdp == NULL) {
        log_error("cannot map RSDT table physical address.");
        return NULL;
    }
    dump_table_header(rsdt_base_addr, rsdp);

    if (rsdp->revision > 1 && rsdp->xsdt_addr) {
        roor_table_phy = rsdp->xsdt_addr;
        table_entries  = 8;
    } else {
        roor_table_phy = rsdp->rsdt_addr;
        table_entries  = 4;
    }
    neo_acpi_kernel_unmap(rsdp, sizeof(struct acpi_rsdp));

    return neo_acpi_rsdt_init(handle, roor_table_phy, table_entries);
}

bool table_find_by_signature(const neo_acpi_handle_t *handle, const char signature[4],
                             acpi_table *table) {
    if (!handle || !table) return false;

    uint32_t target_sig = 0;
    neo_acpi_memcpy(&target_sig, signature, sizeof(uint32_t));

    for (size_t i = 0; i < handle->entries_length; i++) {
        const neo_acpi_table_entry_t *entry = handle->entries[i];
        if (entry == NULL) continue;
        if (entry->signature.id == target_sig) {
            table->index     = i;
            table->phys_addr = entry->phys_addr;
            table->virt_addr = (uintptr_t)neo_acpi_kernel_map(entry->phys_addr, entry->length);
            return true;
        }
    }
    return false;
}
