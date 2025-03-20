#pragma once

#define HEADER_SIZE 36

#define ACPI_TABLE_RSDT ((void *)rsdt)
#define ACPI_TABLE_FACP ((void *)facp)

#include "ctypes.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t addressid;
    uint8_t register_bitwidth;
    uint8_t register_bitoffset;
    uint8_t access_size;
    uint64_t address;
} acpi_address_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t revision;
    uint32_t rsdt;
} acpi_rsdptr_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4]; // signature "RSDT"
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t *entry;
} acpi_rsdt_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint8_t definition_block;
} acpi_dsdt_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];
    uint32_t length;
    uint8_t FADT_major_version;
    uint8_t checksum;
    uint8_t ome_id[6];
    uint8_t oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;

    uint32_t FIRMWARE_CTRL;
    acpi_dsdt_t *DSDT;

    // no longer in use
    uint8_t unused0;

    uint8_t PM_profile;
    uint16_t SCI_INT;
    uint32_t SMI_CMD;
    uint8_t ACPI_ENABLE;
    uint8_t ACPI_DISABLE;
    uint8_t S4BIOS_REQ;
    uint8_t PSTATE_CNT;
    uint32_t PM1a_EVT_BLK;
    uint32_t PM1b_EVT_BLK;
    uint32_t PM1a_CNT_BLK;
    uint32_t PM1b_CNT_BLK;
    uint32_t PM2_CNT_BLK;
    uint32_t PM_TMR_BLK;
    uint32_t GPE0_BLK;
    uint32_t GPE1_BLK;
    uint8_t PM1_EVT_LEN;
    uint8_t PM1_CNT_LEN;
    uint8_t PM2_CNT_LEN;
    uint8_t PM_TMR_LEN;
    uint8_t GPE0_BLK_LEN;
    uint8_t GPE1_BLK_LEN;
    uint8_t GPE1_BASE;
    uint8_t CST_CNT;
    uint16_t P_LVL2_LAT;
    uint16_t P_LVL3_LAT;
    uint16_t FLUSH_SIZE;
    uint16_t FLUSH_STRIDE;
    uint8_t DUTY_OFFSET;
    uint8_t DUTY_WIDTH;
    uint8_t DAY_ALRM;
    uint8_t MON_ALRM;
    uint8_t CENTURY;

    // reserved in ACPI 1.0,used in since ACPI 2.0+
    uint16_t IAPC_BOOT_ARCH;
    uint8_t unused1;
    uint32_t flags;

    acpi_address_t RESET_REG;
    uint8_t RESET_VALUE;
    uint16_t ARM_BOOT_ARCH;
    uint8_t FADT_minjor_version;

    // there used in 64bits address mode
    uint64_t *X_FIREWARE_CTRL;
    uint64_t *X_DSDT;
    acpi_address_t X_PM1a_EVT_BLK;
    acpi_address_t X_PM1b_EVT_BLK;
    acpi_address_t X_PM1a_CNT_BLK;
    acpi_address_t X_PM1b_CNT_BLK;
    acpi_address_t X_PM2_CNT_BLK;
    acpi_address_t X_PM_TMR_BLK;
    acpi_address_t X_GPE0_BLK;
    acpi_address_t X_GPE1_BLK;
    acpi_address_t SLEEP_CONTROL_REG;
    acpi_address_t SLEEP_STATUS_REG;
    uint8_t hypervisor_vendor_id;
} acpi_facp_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t sign[4]; // string 'APIC'
    uint32_t len;
    uint8_t revision;
    uint8_t chksum;
    uint8_t oemid[6];
    uint8_t oemtableid[8];
    uint8_t oemrevision[4];
    uint8_t create_id[4];
    uint8_t create_revision[4];
    uint32_t local_apic; // local apic address
    uint32_t flags;      // flags =1 install DUAL PIC hardware
} acpi_madt_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_base; // iobase base
    uint32_t gsi_base;
} madt_ioapic_entry_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t bus;
    uint8_t irq;
    uint32_t gsi;
    uint16_t flags;
} madt_ioapic_int_assert_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t nmi;
    uint8_t reserved;
    uint16_t flags;
    uint32_t gsi;
} madt_ioapic_nmi_assert_t;
#pragma pack(pop)

typedef enum {
    MADT_PROCESSOR_LOCAL_APIC = 0,
    MADT_PRCESSOR_IOAPIC = 1,
    MADT_LOCAL_INT_ASSERT = 2,
    MADT_LOCAL_NMI_ASSERT = 3,
} madt_entry_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t ACPI_processorID;
    uint8_t APIC_ID;
    uint32_t flags; // bit0 = processor enabled  bit1 = online capable
} madt_processor_localAPIC_t;
#pragma pack(pop)

typedef struct {
    char sign[4];
    uint32_t len;
    char revision;
    char checksum;
    char oemid[6];
    uint64_t oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) MADT;
typedef struct {
    uint64_t configurationAndCapability;
    uint64_t comparatorValue;
    uint64_t fsbInterruptRoute;
    uint64_t unused;
} __attribute__((packed)) HpetTimer;

typedef struct {
    uint64_t generalCapabilities;
    uint64_t reserved0;
    uint64_t generalConfiguration;
    uint64_t reserved1;
    uint64_t generalIntrruptStatus;
    uint8_t reserved3[0xc8];
    uint64_t mainCounterValue;
    uint64_t reserved4;
    HpetTimer timers[0];
} __attribute__((packed)) volatile HpetInfo;

typedef struct {
    uint8_t addressSpaceID;
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t accessWidth;  //  acpi 3.0
    uintptr_t address;
} __attribute__((packed)) AcpiAddress;
typedef struct {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem[6];
    uint8_t oemTableID[8];
    uint32_t oemVersion;
    uint32_t creatorID;
    uint32_t creatorVersion;
    uint8_t hardwareRevision;
    uint8_t comparatorCount: 5;
    uint8_t counterSize: 1;
    uint8_t reserved: 1;
    uint8_t legacyReplacement: 1;
    uint16_t pciVendorId;
    AcpiAddress hpetAddress;
    uint8_t hpetNumber;
    uint16_t minimumTick;
    uint8_t pageProtection;
} __attribute__((packed)) HPET;

int acpi_enable();
int acpi_disable();
uint8_t *AcpiGetRSDPtr();
uint8_t *AcpiCheckRSDPtr(void *ptr);
int AcpiCheckHeader(void *ptr, uint8_t *sign);
uint32_t AcpiGetMadtBase();
void hpet_initialize();
void acpi_install();

uint32_t nanoTime(); // 获取自ACPI启用后的纳秒数
void usleep(uint32_t nano); //纳秒精度的睡眠
void power_reset(); //重启
void power_off(); //关机