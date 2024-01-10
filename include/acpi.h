#ifndef CPOS_ACPI_H
#define CPOS_ACPI_H

#include <stdint.h>

struct ACPI_RSDP {
    char Signature[8];
    unsigned char Checksum;
    char OEMID[6];
    unsigned char Revision;
    unsigned int RsdtAddress;
    unsigned int Length;
    unsigned int XsdtAddress[2];
    unsigned char ExtendedChecksum;
    unsigned char Reserved[3];
};
struct ACPISDTHeader {
    char Signature[4];
    unsigned int Length;
    unsigned char Revision;
    unsigned char Checksum;
    char OEMID[6];
    char OEMTableID[8];
    unsigned int OEMRevision;
    unsigned int CreatorID;
    unsigned int CreatorRevision;
};
struct ACPI_RSDT {
    struct ACPISDTHeader header;
    unsigned int Entry;
};
typedef struct {
    unsigned char AddressSpace;
    unsigned char BitWidth;
    unsigned char BitOffset;
    unsigned char AccessSize;
    unsigned int Address[2];
} GenericAddressStructure;

struct ACPI_FADT {
    struct ACPISDTHeader h;
    unsigned int FirmwareCtrl;
    unsigned int Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    unsigned char Reserved;

    unsigned char PreferredPowerManagementProfile;
    unsigned short SCI_Interrupt;
    unsigned int SMI_CommandPort;
    unsigned char AcpiEnable;
    unsigned char AcpiDisable;
    unsigned char S4BIOS_REQ;
    unsigned char PSTATE_Control;
    unsigned int PM1aEventBlock;
    unsigned int PM1bEventBlock;
    unsigned int PM1aControlBlock;
    unsigned int PM1bControlBlock;
    unsigned int PM2ControlBlock;
    unsigned int PMTimerBlock;
    unsigned int GPE0Block;
    unsigned int GPE1Block;
    unsigned char PM1EventLength;
    unsigned char PM1ControlLength;
    unsigned char PM2ControlLength;
    unsigned char PMTimerLength;
    unsigned char GPE0Length;
    unsigned char GPE1Length;
    unsigned char GPE1Base;
    unsigned char CStateControl;
    unsigned short WorstC2Latency;
    unsigned short WorstC3Latency;
    unsigned short FlushSize;
    unsigned short FlushStride;
    unsigned char DutyOffset;
    unsigned char DutyWidth;
    unsigned char DayAlarm;
    unsigned char MonthAlarm;
    unsigned char Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    unsigned short BootArchitectureFlags;

    unsigned char Reserved2;
    unsigned int Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    unsigned char ResetValue;
    unsigned char Reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    unsigned int X_FirmwareControl[2];
    unsigned int X_Dsdt[2];

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} __attribute__((packed));

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
} __attribute__((packed)) HpetInfo;
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
    uint8_t comparatorCount : 5;
    uint8_t counterSize : 1;
    uint8_t reserved : 1;
    uint8_t legacyReplacement : 1;
    uint16_t pciVendorId;
    AcpiAddress hpetAddress;
    uint8_t hpetNumber;
    uint16_t minimumTick;
    uint8_t pageProtection;
} __attribute__((packed)) HPET;

unsigned int* acpi_find_rsdp(void);
char checksum(unsigned char* addr, unsigned int length);
unsigned int acpi_find_table(char* Signature);
int acpi_shutdown(void);

void hpet_initialize();
uint64_t nanoTime();
void usleep(uint64_t nano);

void acpi_install();

#endif
