#pragma once

#define ACPI_RSDP_SIGNATURE "RSD PTR "
#define ACPI_RSDT_SIGNATURE "RSDT"
#define ACPI_XSDT_SIGNATURE "XSDT"
#define ACPI_MADT_SIGNATURE "APIC"
#define ACPI_FADT_SIGNATURE "FACP"
#define ACPI_FACS_SIGNATURE "FACS"
#define ACPI_MCFG_SIGNATURE "MCFG"
#define ACPI_HPET_SIGNATURE "HPET"
#define ACPI_SRAT_SIGNATURE "SRAT"
#define ACPI_SLIT_SIGNATURE "SLIT"
#define ACPI_DSDT_SIGNATURE "DSDT"
#define ACPI_SSDT_SIGNATURE "SSDT"
#define ACPI_PSDT_SIGNATURE "PSDT"
#define ACPI_ECDT_SIGNATURE "ECDT"
#define ACPI_RHCT_SIGNATURE "RHCT"

#include "neotype.h"

typedef union acpi_object_name {
    char     text[4];
    uint32_t id;
} acpi_object_name;

struct acpi_rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_addr;

    // vvvv available if .revision >= 2.0 only
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  extended_checksum;
    uint8_t  rsvd[3];
} __attribute__((packed));

struct acpi_sdt_hdr {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

typedef struct acpi_table {
    union {
        uintptr_t            virt_addr;
        void                *ptr;
        struct acpi_sdt_hdr *hdr;
    };
    neo_acpi_phys_addr phys_addr;
    size_t             index;
} acpi_table;

struct acpi_gas {
    uint8_t  address_space_id;
    uint8_t  register_bit_width;
    uint8_t  register_bit_offset;
    uint8_t  access_size;
    uint64_t address;
} __attribute__((packed));

struct acpi_rxsdt {
    struct acpi_sdt_hdr hdr;
    uint8_t             ptr_bytes[];
} __attribute__((packed));

typedef struct acpi_fadt_info {
    const char *name;
    uint16_t address64;
    uint16_t address32;
    uint16_t length;
    uint8_t default_length;
    uint8_t flags;
} acpi_fadt_info;

struct acpi_hpet {
    struct acpi_sdt_hdr hdr;
    uint32_t            block_id;
    struct acpi_gas     address;
    uint8_t             number;
    uint16_t            min_clock_tick;
    uint8_t             flags;
} __attribute__((packed));

void dump_table_header(neo_acpi_phys_addr phys_addr, void *hdr);

