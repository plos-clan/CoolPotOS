#include "../include/acpi.h"
#include "../include/memory.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/isr.h"
#include "../include/timer.h"
#include "../include/common.h"

uint16_t SLP_TYPa;
uint16_t SLP_TYPb;
uint32_t SMI_CMD;
uint8_t ACPI_ENABLE;
uint8_t ACPI_DISABLE;
uint32_t *PM1a_CNT;
uint32_t *PM1b_CNT;
uint8_t PM1_CNT_LEN;
uint16_t SLP_EN;
uint16_t SCI_EN;

acpi_rsdt_t *rsdt; // root system descript table
acpi_facp_t *facp; // fixed ACPI table

int acpi_enable_flag;
uint8_t *rsdp_address;

int acpi_enable() {
    int i;

    // check if enable ACPI
    if (io_in16((uint32_t) PM1a_CNT) & SCI_EN) {
        printf("ACPI already enable!\n");
        return 0;
    }

    if (SMI_CMD && ACPI_ENABLE) {
        // send ACPI_ENABLE cmd
        io_out8((uint16_t)
        SMI_CMD, ACPI_ENABLE);

        // wait enable
        for (i = 0; i < 300; i++) {
            if (io_in16((uint32_t) PM1a_CNT) & SCI_EN) {
                break;
            }
            clock_sleep(5);
        }
        // wait enable
        if (PM1b_CNT) {
            for (i = 0; i < 300; i++) {
                if (io_in16((uint32_t) PM1b_CNT) & SCI_EN) {
                    break;
                }
                clock_sleep(5);
            }
        }
        // check enable status
        if (i < 300) {
            printf("[acpi]: Enable ACPI\n");
            return 0;
        } else {
            printf("Counld't enable ACPI\n");
            return -1;
        }
    }
    return -1;
}

int acpi_disable() {
    int i;

    if (!io_in16((uint16_t)PM1a_CNT) &SCI_EN)
    return 0;

    if (SMI_CMD || ACPI_DISABLE) {
        io_out8((uint16_t)
        SMI_CMD, ACPI_DISABLE);

        for (i = 0; i < 300; i++) {
            if (!io_in16((uint16_t)PM1a_CNT) &SCI_EN)
            break;
            clock_sleep(5);
        }

        for (i = 0; i < 300; i++) {
            if (!io_in16((uint16_t)PM1b_CNT) &SCI_EN)
            break;
            clock_sleep(5);
        }

        if (i < 300) {
            printf("ACPI Disable!\n");
            return 0;
        } else {
            printf("Could't disable ACPI\n");
            return -1;
        }
    }
    return -1;
}

uint8_t *AcpiGetRSDPtr() {
    uint32_t *addr;
    uint32_t *rsdt;
    uint32_t ebda;

    for (addr = (uint32_t *) 0x000E0000; addr < (uint32_t *) 0x00100000; addr += 0x10 / sizeof(addr)) {
        rsdt = AcpiCheckRSDPtr(addr);
        if (rsdt) {
            return rsdt;
        }
    }

    // search extented bios data area for the root system description pointer signature
    ebda = *(uint16_t * )
    0x40E;
    ebda = ebda * 0x10 & 0xfffff;
    for (addr = (uint32_t *) ebda; addr < (uint32_t * )(ebda + 1024); addr += 0x10 / sizeof(addr)) {
        rsdt = AcpiCheckRSDPtr(addr);
        if (rsdt) {
            return rsdt;
        }
    }
    return NULL;
}

void power_reset() {
    uint8_t val;
    if (!SCI_EN)
        return;
    while (1) {
        // write ICH port
        io_out8(0x92, 0x01);
        // send RESET_VAL
        io_out8((uint32_t) facp->RESET_REG.address, facp->RESET_VALUE);
    }
}

void power_off() {
    if (!SCI_EN)
        return;
    while (1) {
        printf("[acpi] send power off command!\n");
        io_out16((uint32_t) PM1a_CNT, SLP_TYPa | SLP_EN);
        if (!PM1b_CNT) {
            io_out16((uint32_t) PM1b_CNT, SLP_TYPb | SLP_EN);
        }
    }
}

static int AcpiPowerHandler(registers_t *irq) {
    uint16_t status = io_in16((uint32_t) facp->PM1a_EVT_BLK);
    // check if power button press
    if (status & (1 << 8)) {
        io_out16((uint32_t) facp->PM1a_EVT_BLK, status &= ~(1 << 8)); // clear bits
        printf("Shutdown OS...");
        shutdown_kernel();
        return 0;
    }
    if (!facp->PM1b_EVT_BLK)
        return -1;
    // check if power button press
    status = io_in16((uint32_t) facp->PM1b_EVT_BLK);
    if (status & (1 << 8)) {
        io_out16((uint32_t) facp->PM1b_EVT_BLK, status &= ~(1 << 8));
        printf("Shutdown OS...");
        shutdown_kernel();
        return 0;
    }
    return -1;
}

static void AcpiPowerInit() {
    uint32_t len = facp->PM1_EVT_LEN / 2;
    uint32_t *PM1a_ENABLE_REG = facp->PM1a_EVT_BLK + len;
    uint32_t *PM1b_ENABLE_REG = facp->PM1b_EVT_BLK + len;

    if (!facp)
        return;

    io_out16((uint16_t)PM1a_ENABLE_REG, (uint8_t)(1 << 8));
    if (PM1b_ENABLE_REG) {
        io_out16((uint16_t)PM1b_ENABLE_REG, (uint8_t)(1 << 8));
    }

    printf("ACPI : SCI_INT %08x\n",(uint8_t)facp->SCI_INT);

    register_interrupt_handler(facp->SCI_INT, AcpiPowerHandler);
}

int AcpiCheckHeader(void *ptr, uint8_t *sign) {
    uint8_t * bptr = ptr;
    uint32_t len = *(bptr + 4);
    uint8_t checksum = 0;

    if (!memcmp(bptr, sign, 4)) {
        while (len-- > 0) {
            checksum += *bptr++;
        }
        return 0;
    }
    return -1;
}

uint8_t *AcpiCheckRSDPtr(void *ptr) {
    char *sign = "RSD PTR ";
    acpi_rsdptr_t *rsdp = ptr;
    uint8_t * bptr = ptr;
    uint32_t i = 0;
    uint8_t check = 0;

    // check signature
    if (!memcmp(sign, bptr, 8)) {
        printf("[acpi] rsdp found at %0x\n", bptr);
        rsdp_address = bptr;
        for (i = 0; i < sizeof(acpi_rsdptr_t); i++) {
            check += *bptr;
            bptr++;
        }
        if (!check) {
            return (uint8_t * )
            rsdp->rsdt;
        }
    }
    return NULL;
}

uint32_t AcpiGetMadtBase() {
    uint32_t entrys = rsdt->length - HEADER_SIZE / 4;
    uint32_t **p = &(rsdt->entry);
    acpi_madt_t *madt = NULL;

    while (--entrys) {
        if (!AcpiCheckHeader(*p, "APIC")) {
            madt = (acpi_madt_t *) *p;
            return madt;
        }
        p++;
    }
    return 0;
}

static int AcpiSysInit() {

    uint32_t **p;
    uint32_t entrys;
    uint32_t dsdtlen;

    rsdt = (acpi_rsdt_t *) AcpiGetRSDPtr();
    if (!rsdt || AcpiCheckHeader(rsdt, "RSDT") < 0) {
        printf("No ACPI\n");
        return -1;
    }

    entrys = rsdt->length - HEADER_SIZE / 4;
    p = &(rsdt->entry);
    while (entrys--) {
        if (!AcpiCheckHeader(*p, "FACP")) {
            facp = (acpi_facp_t *) *p;

            ACPI_ENABLE = facp->ACPI_ENABLE;
            ACPI_DISABLE = facp->ACPI_DISABLE;

            SMI_CMD = facp->SMI_CMD;

            PM1a_CNT = facp->PM1a_CNT_BLK;
            PM1b_CNT = facp->PM1b_CNT_BLK;

            PM1_CNT_LEN = facp->PM1_CNT_LEN;

            SLP_EN = 1 << 13;
            SCI_EN = 1;

            uint8_t * S5Addr;
            uint32_t dsdtlen;

            if (!AcpiCheckHeader(facp->DSDT, "DSDT")) {
                S5Addr = &(facp->DSDT->definition_block);
                dsdtlen = facp->DSDT->length - HEADER_SIZE;
                while (dsdtlen--) {
                    if (!memcmp(S5Addr, "_S5_", 4)) {
                        break;
                    }
                    S5Addr++;
                }

                if (dsdtlen) {
                    // check valid \_S5
                    if (*(S5Addr - 1) == 0x08 || (*(S5Addr - 2) == 0x08 && *(S5Addr - 1) == '\\')) {
                        S5Addr += 5;
                        S5Addr += ((*S5Addr & 0xC0) >> 6) + 2;

                        if (*S5Addr == 0x0A) {
                            S5Addr++;
                        }
                        SLP_TYPa = *(S5Addr) << 10;
                        S5Addr++;
                        if (*S5Addr == 0x0A) {
                            S5Addr++;
                        }
                        SLP_TYPb = *(S5Addr) << 10;
                        S5Addr++;
                    } else {
                        printf("[acpi] \\_S5 parse error!\n");
                    }
                } else {
                    printf("[acpi] \\_S5 not present!\n");
                }
            } else {
                printf("[acpi] no found DSDT table\n");
            }
            return 0;
        }
        ++p;
    }
    printf("[acpi] No valid FACP present\n");
}

void acpi_install() {
    AcpiSysInit();
    acpi_enable_flag = !acpi_enable();
    // power init
    AcpiPowerInit();
}