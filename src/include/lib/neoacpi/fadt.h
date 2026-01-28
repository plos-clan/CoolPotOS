#pragma once

#define ACPI_FADT_WBINVD          (1)      /* 00: [V1] The WBINVD instruction works properly */
#define ACPI_FADT_WBINVD_FLUSH    (1 << 1) /* 01: [V1] WBINVD flushes but does not invalidate caches */
#define ACPI_FADT_C1_SUPPORTED    (1 << 2) /* 02: [V1] All processors support C1 state */
#define ACPI_FADT_C2_MP_SUPPORTED (1 << 3) /* 03: [V1] C2 state works on MP system */
#define ACPI_FADT_POWER_BUTTON                                                                     \
    (1 << 4) /* 04: [V1] Power button is handled as a control method device */
#define ACPI_FADT_SLEEP_BUTTON                                                                     \
    (1 << 5) /* 05: [V1] Sleep button is handled as a control method device */
#define ACPI_FADT_FIXED_RTC         (1 << 6) /* 06: [V1] RTC wakeup status is not in fixed register space */
#define ACPI_FADT_S4_RTC_WAKE       (1 << 7) /* 07: [V1] RTC alarm can wake system from S4 */
#define ACPI_FADT_32BIT_TIMER       (1 << 8) /* 08: [V1] ACPI timer width is 32-bit (0=24-bit) */
#define ACPI_FADT_DOCKING_SUPPORTED (1 << 9) /* 09: [V1] Docking supported */
#define ACPI_FADT_RESET_REGISTER                                                                   \
    (1 << 10) /* 10: [V2] System reset via the FADT RESET_REG supported */
#define ACPI_FADT_SEALED_CASE                                                                      \
    (1 << 11) /* 11: [V3] No internal expansion capabilities and case is sealed */
#define ACPI_FADT_HEADLESS                                                                         \
    (1 << 12) /* 12: [V3] No local video capabilities or local input devices */
#define ACPI_FADT_SLEEP_TYPE                                                                       \
    (1 << 13) /* 13: [V3] Must execute native instruction after writing  SLP_TYPx register */
#define ACPI_FADT_PCI_EXPRESS_WAKE                                                                 \
    (1 << 14) /* 14: [V4] System supports PCIEXP_WAKE (STS/EN) bits (ACPI 3.0) */
#define ACPI_FADT_PLATFORM_CLOCK                                                                   \
    (1 << 15) /* 15: [V4] OSPM should use platform-provided timer (ACPI 3.0) */
#define ACPI_FADT_S4_RTC_VALID                                                                     \
    (1 << 16) /* 16: [V4] Contents of RTC_STS valid after S4 wake (ACPI 3.0) */
#define ACPI_FADT_REMOTE_POWER_ON                                                                  \
    (1 << 17) /* 17: [V4] System is compatible with remote power on (ACPI 3.0) */
#define ACPI_FADT_APIC_CLUSTER                                                                     \
    (1 << 18) /* 18: [V4] All local APICs must use cluster model (ACPI 3.0) */
#define ACPI_FADT_APIC_PHYSICAL                                                                    \
    (1 << 19) /* 19: [V4] All local xAPICs must use physical dest mode (ACPI 3.0) */
#define ACPI_FADT_HW_REDUCED (1 << 20) /* 20: [V5] ACPI hardware is not implemented (ACPI 5.0) */
#define ACPI_FADT_LOW_POWER_S0                                                                     \
    (1 << 21) /* 21: [V5] S0 power savings are equal or better than S3 (ACPI 5.0) */

#define ACPI_FADT_OPTIONAL        0
#define ACPI_FADT_REQUIRED        1
#define ACPI_FADT_SEPARATE_LENGTH 2
#define ACPI_FADT_GPE_REGISTER    4

#define ACPI_FADT_OFFSET(f) (uint16_t)offsetof(struct acpi_fadt, f)

#define ACPI_FADT_V1_SIZE (uint32_t)(ACPI_FADT_OFFSET(flags) + 4)
#define ACPI_FADT_V2_SIZE (uint32_t)(ACPI_FADT_OFFSET(fadt_minor_verison) + 1)
#define ACPI_FADT_V3_SIZE (uint32_t)(ACPI_FADT_OFFSET(sleep_control_reg))
#define ACPI_FADT_V5_SIZE (uint32_t)(ACPI_FADT_OFFSET(hypervisor_vendor_identity))
#define ACPI_FADT_V6_SIZE (uint32_t)(sizeof(struct acpi_fadt))

#define ACPI_FADT_INFO_ENTRIES (sizeof(fadt_info_table) / sizeof(acpi_fadt_info_t))

#include "neotype.h"

typedef struct acpi_fadt_info {
    const char *name;
    uint16_t    addr64;
    uint16_t    addr32;
    uint16_t    length;
    uint8_t     default_length;
    uint8_t     flags;
} acpi_fadt_info_t;
