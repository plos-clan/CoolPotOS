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

/* Number of distinct FADT-based GPE register blocks (GPE0 and GPE1) */

#define ACPI_MAX_GPE_BLOCKS 2

/* Default ACPI register widths */

#define ACPI_GPE_REGISTER_WIDTH   8
#define ACPI_PM1_REGISTER_WIDTH   16
#define ACPI_PM2_REGISTER_WIDTH   8
#define ACPI_PM_TIMER_WIDTH       32
#define ACPI_RESET_REGISTER_WIDTH 8

/* Names within the namespace are 4 bytes long */

#define ACPI_NAMESEG_SIZE        4 /* Fixed by ACPI spec */
#define ACPI_PATH_SEGMENT_LENGTH 5 /* 4 chars for name + 1 char for separator */
#define ACPI_PATH_SEPARATOR      '.'

/* Sizes for ACPI table headers */

#define ACPI_OEM_ID_SIZE       6
#define ACPI_OEM_TABLE_ID_SIZE 8

/* ACPI/PNP hardware IDs */

#define PCI_ROOT_HID_STRING         "PNP0A03"
#define PCI_EXPRESS_ROOT_HID_STRING "PNP0A08"

/* PM Timer ticks per second (HZ) */

#define ACPI_PM_TIMER_FREQUENCY 3579545

#define ACPI_PM1_CNT_SCI_EN (1 << 0)

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

struct acpi_hpet {
    struct acpi_sdt_hdr hdr;
    uint32_t            block_id;
    struct acpi_gas     address;
    uint8_t             number;
    uint16_t            min_clock_tick;
    uint8_t             flags;
} __attribute__((packed));

struct acpi_fadt {
    struct acpi_sdt_hdr hdr;
    uint32_t            firmware_ctrl;
    uint32_t            dsdt;
    uint8_t             int_model;
    uint8_t             preferred_pm_profile;
    uint16_t            sci_int;
    uint32_t            smi_cmd;
    uint8_t             acpi_enable;
    uint8_t             acpi_disable;
    uint8_t             s4bios_req;
    uint8_t             pstate_cnt;
    uint32_t            pm1a_evt_blk;
    uint32_t            pm1b_evt_blk;
    uint32_t            pm1a_cnt_blk;
    uint32_t            pm1b_cnt_blk;
    uint32_t            pm2_cnt_blk;
    uint32_t            pm_tmr_blk;
    uint32_t            gpe0_blk;
    uint32_t            gpe1_blk;
    uint8_t             pm1_evt_len;
    uint8_t             pm1_cnt_len;
    uint8_t             pm2_cnt_len;
    uint8_t             pm_tmr_len;
    uint8_t             gpe0_blk_len;
    uint8_t             gpe1_blk_len;
    uint8_t             gpe1_base;
    uint8_t             cst_cnt;
    uint16_t            p_lvl2_lat;
    uint16_t            p_lvl3_lat;
    uint16_t            flush_size;
    uint16_t            flush_stride;
    uint8_t             duty_offset;
    uint8_t             duty_width;
    uint8_t             day_alrm;
    uint8_t             mon_alrm;
    uint8_t             century;
    uint16_t            iapc_boot_arch;
    uint8_t             _rsvd;
    uint32_t            flags;
    struct acpi_gas     reset_reg;
    uint8_t             reset_value;
    uint16_t            arm_boot_arch;
    uint8_t             fadt_minor_verison;
    uint64_t            x_firmware_ctrl;
    uint64_t            x_dsdt;
    struct acpi_gas     x_pm1a_evt_blk;
    struct acpi_gas     x_pm1b_evt_blk;
    struct acpi_gas     x_pm1a_cnt_blk;
    struct acpi_gas     x_pm1b_cnt_blk;
    struct acpi_gas     x_pm2_cnt_blk;
    struct acpi_gas     x_pm_tmr_blk;
    struct acpi_gas     x_gpe0_blk;
    struct acpi_gas     x_gpe1_blk;
    struct acpi_gas     sleep_control_reg;
    struct acpi_gas     sleep_status_reg;
    uint64_t            hypervisor_vendor_identity;
} __attribute__((packed));

struct acpi_dsdt {
    struct acpi_sdt_hdr hdr;
    uint8_t             definition_block[];
} __attribute__((packed));

struct acpi_ssdt {
    struct acpi_sdt_hdr hdr;
    uint8_t             definition_block[];
} __attribute__((packed));

typedef struct acpi_generic_address {
    uint8_t  space_id;     /* Address space where struct or register exists */
    uint8_t  bit_width;    /* Size in bits of given register */
    uint8_t  bit_offset;   /* Bit offset within the register */
    uint8_t  access_width; /* Minimum Access size (ACPI 3.0) */
    uint64_t address;      /* 64-bit address of struct or register */
} acpi_generic_address_t;

typedef struct acpi_table_facs {
    char     signature[4];       /* ASCII table signature */
    uint32_t length;             /* Length of structure, in bytes */
    uint32_t hard_signature;     /* Hardware configuration signature */
    uint32_t firm_waking_vector; /* 32-bit physical address of the Firmware Waking Vector */
    uint32_t global_lock;        /* Global Lock for shared hardware resources */
    uint32_t flags;
    uint64_t x_firm_waking_vector; /* 64-bit version of the Firmware Waking Vector (ACPI 2.0+) */
    uint8_t  version;              /* Version of this table (ACPI 2.0+) */
    uint8_t  _reserved[3];         /* Reserved, must be zero */
    uint32_t ospm_flags;           /* Flags to be set by OSPM (ACPI 4.0) */
    uint8_t  _reserved1[24];       /* Reserved, must be zero */
} __attribute__((packed)) acpi_table_facs_t;

void dump_table_header(neo_acpi_phys_addr phys_addr, void *hdr);
bool verify_table_checksum(void *table, size_t size);
