#include "../include/acpi.h"
#include "../include/memory.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/isr.h"
#include "../include/timer.h"
#include "../include/common.h"
#include "../include/task.h"

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
uint32_t PM1a_EVT_BLK;
uint32_t PM1b_EVT_BLK;

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
            return 0;
        } else {
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
            return 0;
        } else {
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
        io_out16((uint32_t) PM1a_CNT, SLP_TYPa | SLP_EN);
        if (!PM1b_CNT) {
            io_out16((uint32_t) PM1b_CNT, SLP_TYPb | SLP_EN);
        }
    }
}

static int AcpiPowerHandler(registers_t *irq) {
    io_cli();
    uint16_t status = io_in16((uint32_t) PM1a_EVT_BLK);

    // 不检查电源键, 按下就关机

    io_out16((uint32_t) PM1a_EVT_BLK, status &= ~(1 << 8)); // clear bits
    printf("Shutdown OS...");
    shutdown_kernel();

    // check if power button press
    if (status & (1 << 8)) {

        return 0;
    }
    if (!PM1b_EVT_BLK){
        printf("PowerDown fault\n");
        io_sti();
        return -1;
    }

    printf("DEBUG IV\n");
    // check if power button press
    status = io_in16((uint32_t) PM1b_EVT_BLK);
    printf("DEBUG V\n");
    if (status & (1 << 8)) {
        io_out16((uint32_t) PM1b_EVT_BLK, status &= ~(1 << 8));
        printf("Shutdown OS...");
        shutdown_kernel();
        return 0;
    }
    io_sti();
    return -1;
}

static void AcpiPowerInit() {
    if (!facp) return;

    uint8_t len = facp->PM1_EVT_LEN / 2;
    uint32_t *PM1a_ENABLE_REG = facp->PM1a_EVT_BLK + len;
    uint32_t *PM1b_ENABLE_REG = facp->PM1b_EVT_BLK + len;

    if (PM1b_ENABLE_REG == len)
        PM1b_ENABLE_REG = 0;

    io_out16(PM1a_ENABLE_REG, (1 << 8));
    if (PM1b_ENABLE_REG) {
        io_out16((uint16_t)
        PM1b_ENABLE_REG, (uint8_t)(1 << 8));
    }

    register_interrupt_handler(facp->SCI_INT + 0x20, AcpiPowerHandler);
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
        rsdp_address = bptr;
        for (i = 0; i < sizeof(acpi_rsdptr_t); i++) {
            check += *bptr;
            bptr++;
        }
        if (!check) {
            return (uint8_t * ) rsdp->rsdt;
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
        return false;
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

            PM1a_EVT_BLK = facp->PM1b_EVT_BLK;
            PM1b_EVT_BLK = facp->PM1b_EVT_BLK;

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
            return true;
        }
        ++p;
    }
    printf("[acpi] No valid FACP present\n");
}

void acpi_install() {
    klogf(AcpiSysInit(),"Load acpi driver.\n");
    acpi_enable_flag = !acpi_enable();
    hpet_initialize();
    AcpiPowerInit();
}

static HpetInfo *hpetInfo = NULL;
static uint32_t hpetPeriod = 0;

uint32_t nanoTime() {
    if(hpetInfo == NULL) return 0;
    uint32_t mcv =  hpetInfo->mainCounterValue;
    return mcv * hpetPeriod;
}

void usleep(uint32_t nano) {
    uint32_t targetTime = nanoTime();
    uint32_t after = 0;
    while (1) {
        uint64_t n = nanoTime();
        if (n < targetTime) {
            after += 0xffffffff - targetTime + n;
            targetTime = n;
        } else {
            after += n - targetTime;
            targetTime = n;
        }
        if (after >= nano) {
            return;
        }
    }
}

unsigned int acpi_find_table(char *Signature) {
    uint8_t * ptr, *ptr2;
    uint32_t len;
    uint8_t * rsdt_t = (uint8_t * )
    rsdt;
    // iterate on ACPI table pointers
    for (len = *((uint32_t * )(rsdt_t + 4)), ptr2 = rsdt_t + 36; ptr2 < rsdt_t + len;
         ptr2 += (char *) rsdt_t[0] == 'X' ? 8 : 4) {
        ptr = (uint8_t * )(uintptr_t)(rsdt_t[0] == 'X' ? *((uint64_t *) ptr2)
                                                       : *((uint32_t *) ptr2));
        if (!memcmp(ptr, Signature, 4)) {
            return (unsigned) ptr;
        }
    }
    // printk("not found.\n");
    return 0;
}

static void hpet_clock(registers_t *regs){
    //schedule(regs);
}

void hpet_initialize() {
    HPET *hpet = acpi_find_table("HPET");
    if (!hpet) {
        printf("can not found acpi hpet table\n");
    }

    hpetInfo = (HpetInfo *) hpet->hpetAddress.address;

    uint32_t counterClockPeriod = hpetInfo->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;

   // HpetTimer hpet_timer = hpetInfo->timers[0];
   // hpet_timer.configurationAndCapability = 0x004c;
   // hpet_timer.comparatorValue = 14318179;

   // hpetInfo->mainCounterValue = 0;

   // register_interrupt_handler(0x20,hpet_clock);

    hpetInfo->generalConfiguration |= 1;  //  启用hpet
}