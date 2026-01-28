#include "lib/neoacpi/fadt.h"
#include "lib/neoacpi/neo_impl.h"
#include "lib/neoacpi/neo_logger.h"
#include "lib/neoacpi/neo_stdlib.h"
#include "lib/neoacpi/neoacpi.h"
#include "lib/neoacpi/neotable.h"

bool neo_acpi_push_table(neo_acpi_handle_t *handle, neo_acpi_table_entry_t *data) {
    const size_t             new_len = handle->entries_length + 1;
    neo_acpi_table_entry_t **new_entries =
        (neo_acpi_table_entry_t **)neo_acpi_malloc(new_len * sizeof(void *));

    if ((void *)new_entries == NULL) { return false; }

    if (handle->entries_length > 0 && (void *)handle->entries != NULL) {
        neo_acpi_memcpy((void *)new_entries, (void *)handle->entries,
                        handle->entries_length * sizeof(void *));
        neo_acpi_free((void *)handle->entries);
    }

    new_entries[handle->entries_length] = data;
    handle->entries                     = new_entries;
    handle->entries_length              = new_len;

    return true;
}

static bool signatures_match(const void *const lhs, const void *const rhs) {
    return neo_acpi_memcmp(lhs, rhs, sizeof(acpi_object_name)) == 0;
}

static uint8_t table_checksum(const void *table, const size_t size) {
    const uint8_t *bytes = table;
    uint8_t        csum  = 0;

    for (size_t i = 0; i < size; ++i)
        csum += bytes[i];

    return csum;
}

void dump_table_header(const neo_acpi_phys_addr phys_addr, void *hdr) {
    struct acpi_sdt_hdr *sdt = hdr;

    if (signatures_match(hdr, ACPI_FACS_SIGNATURE)) {
        log_info("FACS 0x%p %08X", phys_addr, sdt->length);
        return;
    }

    if (!neo_acpi_memcmp(hdr, ACPI_RSDP_SIGNATURE, sizeof(ACPI_RSDP_SIGNATURE) - 1)) {
        struct acpi_rsdp *rsdp = hdr;
        log_info("RSDP 0x%p %08X v%02X %6.6s", phys_addr, rsdp->revision >= 2 ? rsdp->length : 20,
                 rsdp->revision, rsdp->oemid);
        return;
    }
    log_info("%.4s 0x%p %08X v%02X %6.6s %8.8s", sdt->signature, phys_addr, sdt->length,
             sdt->revision, sdt->oemid, sdt->oem_table_id);
}

static bool check_table_signature(void *table, const char *expect) {
    if (!signatures_match(table, expect)) {
        struct acpi_sdt_hdr *hdr = table;
        log_error("invalid table '%.4s' (OEM ID '%.6s' OEM Table ID '%.8s') signature (expected "
                  "'%.4s')\n",
                  hdr->signature, hdr->oemid, hdr->oem_table_id, expect);
        return false;
    }
    return true;
}

bool verify_table_checksum(void *table, size_t size) {
    uint8_t csum = table_checksum(table, size);

    if (csum != 0) {
        struct acpi_sdt_hdr *hdr = table;
        log_error("invalid table '%.4s' (OEM ID '%.6s' OEM Table ID '%.8s') checksum %d!\n",
                  (hdr)->signature, (hdr)->oemid, (hdr)->oem_table_id, csum);
        return false;
    }

    return true;
}

static bool load_table_entry(neo_acpi_handle_t *handle, neo_acpi_phys_addr entry_addr) {
    struct acpi_sdt_hdr *hdr = neo_acpi_kernel_map(entry_addr, sizeof(struct acpi_sdt_hdr));
    if (hdr == NULL) {
        log_error("load_table_entry: cannot mmap entry_addr.");
        return false;
    }
    if (!verify_table_checksum(hdr, hdr->length)) {
        neo_acpi_kernel_unmap(hdr, sizeof(struct acpi_sdt_hdr));
        return false;
    }

    neo_acpi_table_entry_t *entry = neo_acpi_malloc(sizeof(neo_acpi_table_entry_t));
    if (entry == NULL) {
        neo_acpi_kernel_unmap(hdr, sizeof(struct acpi_sdt_hdr));
        log_error("load_table_entry: cannot alloc entry.");
        return false;
    }

    neo_acpi_memcpy(&entry->signature.id, hdr->signature, 4);
    entry->phys_addr = entry_addr;
    entry->length    = hdr->length;

    dump_table_header(entry_addr, hdr);

    return neo_acpi_push_table(handle, entry);
}

neo_acpi_handle_t *neo_acpi_rsdt_init(neo_acpi_handle_t *handle, neo_acpi_phys_addr roor_table_phy,
                                      size_t entry_size) {
    struct acpi_rxsdt *rxsdt       = NULL;
    size_t             entry_bytes = 0;
    size_t             map_len     = sizeof(*rxsdt);
    neo_acpi_phys_addr entry_addr  = 0;

    rxsdt = neo_acpi_kernel_map(roor_table_phy, map_len);
    if (rxsdt == NULL) {
        log_error("cannot mmap rxsdt.");
        return NULL;
    }

    dump_table_header(roor_table_phy, rxsdt);

    if (!check_table_signature(rxsdt,
                               entry_size == 8 ? ACPI_XSDT_SIGNATURE : ACPI_RSDT_SIGNATURE)) {
        goto error_out;
    }

    map_len = rxsdt->hdr.length;
    neo_acpi_kernel_unmap(rxsdt, sizeof(*rxsdt));

    // Invalid table length
    if (map_len < sizeof(*rxsdt) + entry_size) return NULL;

    entry_bytes  = map_len - sizeof(*rxsdt);
    entry_bytes &= ~(entry_size - 1);

    rxsdt = neo_acpi_kernel_map(roor_table_phy, map_len);
    if (rxsdt == NULL) return NULL;

    if (!verify_table_checksum(rxsdt, map_len)) goto error_out;

    for (size_t i = 0; i < entry_bytes; i += entry_size) {
        uint64_t entry_phys_addr_large = 0;
        neo_acpi_memcpy(&entry_phys_addr_large, &rxsdt->ptr_bytes[i], entry_size);
        if (!entry_phys_addr_large) continue;
        entry_addr = entry_phys_addr_large;
        if (!load_table_entry(handle, entry_addr)) return NULL;
    }

    neo_acpi_kernel_unmap(rxsdt, map_len);
    return acpi_load_fadt(handle);
error_out:
    neo_acpi_kernel_unmap(rxsdt, map_len);
    return NULL;
}
