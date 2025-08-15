#pragma once

#define MADT_APIC_CPU 0x00
#define MADT_APIC_IO  0x01
#define MADT_APIC_INT 0x02
#define MADT_APIC_NMI 0x03

#define LAPIC_REG_ID            0x20
#define LAPIC_REG_TIMER_CURCNT  0x390
#define LAPIC_REG_TIMER_INITCNT 0x380
#define LAPIC_REG_TIMER         0x320
#define LAPIC_REG_SPURIOUS      0xf0
#define LAPIC_REG_TIMER_DIV     0x3e0
#define LAPIC_TIMER_PERIODIC    (1 << 17)
#define LAPIC_TIMER_MASKED      (1 << 16)

#define APIC_ICR_LOW  0x300
#define APIC_ICR_HIGH 0x310

#include "ctype.h"

typedef struct {
    uint8_t  addressid;
    uint8_t  register_bitwidth;
    uint8_t  register_bitoffset;
    uint8_t  access_size;
    uint64_t address;
} acpi_address_t;

struct ACPISDTHeader {
    char     Signature[4];
    uint32_t Length;
    uint8_t  Revision;
    uint8_t  Checksum;
    char     OEMID[6];
    char     OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
};

typedef struct {
    char     signature[8];      // 签名
    uint8_t  checksum;          // 校验和
    char     oem_id[6];         // OEM ID
    uint8_t  revision;          // 版本
    uint32_t rsdt_address;      // V1: RSDT 地址 (32-bit)
    uint32_t length;            // 结构体长度
    uint64_t xsdt_address;      // V2: XSDT 地址 (64-bit)
    uint8_t  extended_checksum; // 扩展校验和
    uint8_t  reserved[3];       // 保留字段
} __attribute__((packed)) RSDP;

typedef struct {
    struct ACPISDTHeader h;
    uint64_t             PointerToOtherSDT;
} __attribute__((packed)) XSDT;

typedef struct {
    struct ACPISDTHeader h;
    uint32_t             local_apic_address;
    uint32_t             flags;
    void                *entries;
} __attribute__((packed)) MADT;

struct madt_hander {
    uint8_t entry_type;
    uint8_t length;
} __attribute__((packed));

struct madt_io_apic {
    struct madt_hander h;
    uint8_t            apic_id;
    uint8_t            reserved;
    uint32_t           address;
    uint32_t           gsib;
} __attribute__((packed));

struct madt_local_apic {
    struct madt_hander h;
    uint8_t            ACPI_Processor_UID;
    uint8_t            local_apic_id;
    uint32_t           flags;
};

struct madt_interrupt_source {
    struct madt_hander h;
    uint8_t            bus_source; /* typically 0 = ISA */
    uint8_t            irq_source; /* ISA IRQ */
    uint32_t           gsi;        /* mapped GSI */
    uint16_t           flags;      /* polarity/trigger */
} __attribute__((packed));

typedef struct {
    struct ACPISDTHeader h;
    uint8_t              definition_block;
} acpi_dsdt_t;

struct generic_address {
    uint8_t  address_space;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} __attribute__((packed));

struct hpet {
    struct ACPISDTHeader   h;
    uint32_t               event_block_id;
    struct generic_address base_address;
    uint16_t               clock_tick_unit;
    uint8_t                page_oem_flags;
} __attribute__((packed));

typedef struct {
    uint64_t configurationAndCapability;
    uint64_t comparatorValue;
    uint64_t fsbInterruptRoute;
    uint64_t unused;
} __attribute__((packed)) HpetTimer;

typedef struct {
    uint64_t  generalCapabilities;
    uint64_t  reserved0;
    uint64_t  generalConfiguration;
    uint64_t  reserved1;
    uint64_t  generalIntrruptStatus;
    uint8_t   reserved3[0xc8];
    uint64_t  mainCounterValue;
    uint64_t  reserved4;
    HpetTimer timers[];
} __attribute__((packed)) volatile HpetInfo;

typedef struct dsdt_table {
    uint8_t  signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  oem_id[6];
    uint8_t  oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint8_t  definition_block;
} __attribute__((packed)) dsdt_table_t;

typedef struct facp_table {
    struct ACPISDTHeader   h;
    uint32_t               firmware_ctrl;
    uint32_t               dsdt;
    uint8_t                reserved;
    uint8_t                preferred_pm_profile;
    uint16_t               sci_int;
    uint32_t               smi_cmd;
    uint8_t                acpi_enable;
    uint8_t                acpi_disable;
    uint8_t                s4bios_req;
    uint8_t                pstate_cnt;
    uint32_t               pm1a_evt_blk;
    uint32_t               pm1b_evt_blk;
    uint32_t               pm1a_cnt_blk;
    uint32_t               pm1b_cnt_blk;
    uint32_t               pm2_cnt_blk;
    uint32_t               pm_tmr_blk;
    uint32_t               gpe0_blk;
    uint32_t               gpe1_blk;
    uint8_t                pm1_evt_len;
    uint8_t                pm1_cnt_len;
    uint8_t                pm2_cnt_len;
    uint8_t                pm_tmr_len;
    uint8_t                gpe0_blk_len;
    uint8_t                gpe1_blk_len;
    uint8_t                gpe1_base;
    uint8_t                cst_cnt;
    uint16_t               p_lvl2_lat;
    uint16_t               p_lvl3_lat;
    uint16_t               flush_size;
    uint16_t               flush_stride;
    uint8_t                duty_offset;
    uint8_t                duty_width;
    uint8_t                day_alrm;
    uint8_t                mon_alrm;
    uint8_t                century;
    uint16_t               iapc_boot_arch;
    uint8_t                reserved2;
    uint32_t               flags;
    struct generic_address reset_reg;
    uint8_t                reset_value;
    uint8_t                reserved3[3];
    uint64_t               x_firmware_ctrl;
    uint64_t               x_dsdt;
    struct generic_address x_pm1a_evt_blk;
    struct generic_address x_pm1b_evt_blk;
    struct generic_address x_pm1a_cnt_blk;
    struct generic_address x_pm1b_cnt_blk;
    struct generic_address x_pm2_cnt_blk;
    struct generic_address x_pm_tmr_blk;
    struct generic_address x_gpe0_blk;
    struct generic_address x_gpe1_blk;
} __attribute__((packed)) acpi_facp_t;

typedef struct {
    struct ACPISDTHeader Header;
    uint64_t             Reserved;
} __attribute__((packed)) MCFG;

typedef struct {
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed)) MCFG_ENTRY;

typedef struct generic_address       GenericAddress;
typedef struct hpet                  Hpet;
typedef struct madt_hander           MadtHeader;
typedef struct madt_io_apic          MadtIOApic;
typedef struct madt_interrupt_source MadtInterruptSource;
typedef struct madt_local_apic       MadtLocalApic;
typedef struct facp_table            acpi_facp_t;

void acpi_setup();

void apic_setup(MADT *madt);

void pci_setup(MCFG *mcfg0);

void *find_table(const char *name);

uint64_t lapic_id();

/**
 * 发送CPU间中断
 * @param apic_id CPU lapic_id
 * @param command 命令
 */
void send_ipi(uint32_t apic_id, uint32_t command);

void ioapic_add(uint8_t vector, uint32_t irq);

void ioapic_enable(uint8_t vector, uint32_t irq);

void local_apic_init();

void ap_local_apic_init();

void setup_facp(acpi_facp_t *facp);

void hpet_init(Hpet *hpetInfo);
