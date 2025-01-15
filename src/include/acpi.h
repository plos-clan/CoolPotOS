#pragma once

#define MADT_APIC_CPU 0x00
#define MADT_APIC_IO 0x01
#define MADT_APIC_INT 0x02
#define MADT_APIC_NMI 0x03

#define LAPIC_REG_ID 32
#define LAPIC_REG_TIMER_CURCNT 0x390
#define LAPIC_REG_TIMER_INITCNT 0x380
#define LAPIC_REG_TIMER 0x320
#define LAPIC_REG_SPURIOUS 0xf0
#define LAPIC_REG_TIMER_DIV 0x3e0

#include "ctype.h"

typedef struct {
    uint8_t addressid;
    uint8_t register_bitwidth;
    uint8_t register_bitoffset;
    uint8_t access_size;
    uint64_t address;
} acpi_address_t;

struct ACPISDTHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
};

typedef struct {
    char signature[8];    // 签名
    uint8_t checksum;     // 校验和
    char oem_id[6];       // OEM ID
    uint8_t revision;     // 版本
    uint32_t rsdt_address; // V1: RSDT 地址 (32-bit)
    uint32_t length;      // 结构体长度
    uint64_t xsdt_address; // V2: XSDT 地址 (64-bit)
    uint8_t extended_checksum; // 扩展校验和
    uint8_t reserved[3];  // 保留字段
} __attribute__((packed)) RSDP;

typedef struct {
    struct ACPISDTHeader h;
    uint64_t PointerToOtherSDT;
} __attribute__((packed)) XSDT;

typedef struct {
    struct ACPISDTHeader h;
    uint32_t local_apic_address;
    uint32_t flags;
    void *entries;
} __attribute__((packed)) MADT;

struct madt_hander {
    uint8_t entry_type;
    uint8_t length;
} __attribute__((packed));

struct madt_io_apic {
    struct madt_hander h;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsib;
}__attribute__((packed));

typedef struct {
    struct ACPISDTHeader h;
    uint8_t definition_block;
} acpi_dsdt_t;

struct generic_address {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
}__attribute__((packed));

struct hpet {
    struct ACPISDTHeader h;
    uint32_t event_block_id;
    struct generic_address base_address;
    uint16_t clock_tick_unit;
    uint8_t page_oem_flags;
}__attribute__((packed));

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
    HpetTimer timers[];
} __attribute__((packed)) volatile HpetInfo;

typedef struct generic_address GenericAddress;
typedef struct hpet Hpet;
typedef struct madt_hander MadtHeader;
typedef struct madt_io_apic MadtIOApic;

void acpi_setup();

void apic_setup(MADT *madt);

void *find_table(const char *name);

uint64_t lapic_id();

void ioapic_add(uint8_t vector, uint32_t irq);

void hpet_init(Hpet *hpetInfo);
