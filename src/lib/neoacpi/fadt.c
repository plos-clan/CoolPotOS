#include "lib/neoacpi/fadt.h"
#include "lib/neoacpi/libaml.h"
#include "lib/neoacpi/neo_impl.h"
#include "lib/neoacpi/neo_logger.h"
#include "lib/neoacpi/neo_stdlib.h"
#include "lib/neoacpi/neoacpi.h"
#include "lib/neoacpi/neotable.h"

static acpi_fadt_info_t fadt_info_table[] = {
    {"Pm1aEventBlock",   ACPI_FADT_OFFSET(x_pm1a_evt_blk), ACPI_FADT_OFFSET(pm1a_evt_blk),
     ACPI_FADT_OFFSET(pm1_evt_len),  ACPI_PM1_REGISTER_WIDTH * 2, /* Enable + Status register */
     ACPI_FADT_REQUIRED                            },

    {"Pm1bEventBlock",   ACPI_FADT_OFFSET(x_pm1b_evt_blk), ACPI_FADT_OFFSET(pm1b_evt_blk),
     ACPI_FADT_OFFSET(pm1_evt_len),  ACPI_PM1_REGISTER_WIDTH * 2, /* Enable + Status register */
     ACPI_FADT_OPTIONAL                            },

    {"Pm1aControlBlock", ACPI_FADT_OFFSET(x_pm1a_cnt_blk), ACPI_FADT_OFFSET(pm1a_cnt_blk),
     ACPI_FADT_OFFSET(pm1_cnt_len),  ACPI_PM1_REGISTER_WIDTH,     ACPI_FADT_REQUIRED                                },

    {"Pm1bControlBlock", ACPI_FADT_OFFSET(x_pm1b_cnt_blk), ACPI_FADT_OFFSET(pm1b_cnt_blk),
     ACPI_FADT_OFFSET(pm1_cnt_len),  ACPI_PM1_REGISTER_WIDTH,     ACPI_FADT_OPTIONAL                                },

    {"Pm2ControlBlock",  ACPI_FADT_OFFSET(x_pm2_cnt_blk),  ACPI_FADT_OFFSET(pm2_cnt_blk),
     ACPI_FADT_OFFSET(pm2_cnt_len),  ACPI_PM2_REGISTER_WIDTH,     ACPI_FADT_SEPARATE_LENGTH                         },

    {"PmTimerBlock",     ACPI_FADT_OFFSET(x_pm_tmr_blk),   ACPI_FADT_OFFSET(pm_tmr_blk),
     ACPI_FADT_OFFSET(pm_tmr_len),   ACPI_PM_TIMER_WIDTH,
     ACPI_FADT_SEPARATE_LENGTH                                                                                      }, /* ACPI 5.0A: Timer is optional */

    {"Gpe0Block",        ACPI_FADT_OFFSET(x_gpe0_blk),     ACPI_FADT_OFFSET(gpe0_blk),
     ACPI_FADT_OFFSET(gpe0_blk_len), 0,                           ACPI_FADT_SEPARATE_LENGTH | ACPI_FADT_GPE_REGISTER},

    {"Gpe1Block",        ACPI_FADT_OFFSET(x_gpe1_blk),     ACPI_FADT_OFFSET(gpe1_blk),
     ACPI_FADT_OFFSET(gpe1_blk_len), 0,                           ACPI_FADT_SEPARATE_LENGTH | ACPI_FADT_GPE_REGISTER}
};

neo_acpi_phys_addr neo_acpi_fadt_select(const char *name, uint32_t phy32, uint64_t phy64) {
    if (!phy64) { return phy32; }
    if (phy32 && (phy64 != (uint64_t)phy32)) {
        log_warn("32/64X %s address mismatch in FADT: 0x%08X/0x%016llX, using 64-bit address", name,
                 phy32, (unsigned long long)phy64);
        //TIP select return u32 or default
    }
    return phy64;
}

static uint8_t fadt_calc_bit_width(uint8_t length, uint8_t default_bits) {
    if (length) { return (uint8_t)NEO_ACPI_MIN(255, (uint16_t)length * 8); }
    return default_bits;
}

static uint8_t fadt_calc_byte_length(uint8_t length, uint8_t default_bits) {
    if (length) { return length; }
    if (default_bits) { return (uint8_t)(default_bits / 8); }
    return 0;
}

static void acpi_gas_init_system_io(struct acpi_gas *gas, uint64_t address, uint8_t byte_size) {
    gas->address             = address;
    gas->address_space_id    = 0x01; /* System I/O */
    gas->register_bit_width  = (uint8_t)NEO_ACPI_MIN(255, (uint16_t)byte_size * 8);
    gas->register_bit_offset = 0;
    gas->access_size         = 0;
}

static void acpi_fixup_fadt(struct acpi_fadt *fadt) {
    size_t i;

    if (!fadt->x_dsdt) { fadt->x_dsdt = fadt->dsdt; }
    if (fadt->firmware_ctrl) { fadt->x_firmware_ctrl = fadt->firmware_ctrl; }

    if (fadt->flags & ACPI_FADT_HW_REDUCED) { return; }

    for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
        const acpi_fadt_info_t *info        = &fadt_info_table[i];
        struct acpi_gas        *gas         = ACPI_ADD_PTR(struct acpi_gas, fadt, info->addr64);
        uint32_t                legacy_addr = 0;
        uint8_t                 length      = *ACPI_ADD_PTR(uint8_t, fadt, info->length);
        uint8_t                 bit_width   = fadt_calc_bit_width(length, info->default_length);

        neo_acpi_memcpy(&legacy_addr, ACPI_ADD_PTR(void, fadt, info->addr32), sizeof(legacy_addr));

        if (!gas->address && legacy_addr) {
            uint8_t byte_len = fadt_calc_byte_length(length, info->default_length);
            if (byte_len) { acpi_gas_init_system_io(gas, legacy_addr, byte_len); }
        }

        if (gas->address && bit_width && gas->register_bit_width != bit_width) {
            log_warn("Invalid length for FADT/%s: %u, using %u", info->name,
                     gas->register_bit_width, bit_width);
            gas->register_bit_width = bit_width;
        }
    }

    if (fadt->reset_reg.address &&
        fadt->reset_reg.register_bit_width != ACPI_RESET_REGISTER_WIDTH) {
        log_warn("Invalid length for FADT/ResetReg: %u, using %u",
                 fadt->reset_reg.register_bit_width, ACPI_RESET_REGISTER_WIDTH);
        fadt->reset_reg.register_bit_width = ACPI_RESET_REGISTER_WIDTH;
    }
}

neo_acpi_handle_t *acpi_load_fadt(neo_acpi_handle_t *handle) {
    acpi_table table;
    if (!table_find_by_signature(handle, ACPI_FADT_SIGNATURE, &table)) {
        log_error("cannot find fadt.");
        return NULL;
    }
    struct acpi_fadt *fadt = &handle->global_fadt;

    neo_acpi_memset(fadt, 0, sizeof(*fadt));
    size_t table_length = NEO_ACPI_MIN(table.hdr->length, sizeof(struct acpi_fadt));
    neo_acpi_memcpy(fadt, table.ptr, table_length);

    if (fadt->hdr.length <= ACPI_FADT_V2_SIZE || fadt->hdr.revision <= 2) {
        fadt->cst_cnt              = 0;
        fadt->preferred_pm_profile = 0;
        fadt->pstate_cnt           = 0;
        fadt->iapc_boot_arch       = 0;
    }

    fadt->hdr.length = sizeof(struct acpi_fadt);
    fadt->x_dsdt     = neo_acpi_fadt_select("DSDT", fadt->dsdt, fadt->x_dsdt);

    acpi_fixup_fadt(fadt);

    handle->aml_context = neo_acpi_malloc(sizeof(aml_context_t));
    struct acpi_dsdt *dsdt =
        neo_acpi_kernel_map(handle->global_fadt.x_dsdt, sizeof(struct acpi_sdt_hdr));
    if (dsdt == NULL) {
        log_error("cannot mmap dsdt header table.");
        return NULL;
    }
    size_t dsdt_length = dsdt->hdr.length;
    neo_acpi_kernel_unmap(dsdt, sizeof(struct acpi_sdt_hdr));

    dsdt = neo_acpi_kernel_map(handle->global_fadt.x_dsdt, dsdt_length);
    if (!verify_table_checksum(dsdt, dsdt->hdr.length)) { return NULL; }
    aml_context_initialize(handle, dsdt);
    return handle;
}
