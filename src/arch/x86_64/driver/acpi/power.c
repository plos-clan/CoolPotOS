#include "power.h"
#include "acpi.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "krlibc.h"
#include "timer.h"

uint16_t     SLP_TYPa;
uint16_t     SLP_TYPb;
uint16_t     SLP_EN;
uint16_t     SCI_EN;
acpi_facp_t *facp;

__IRQHANDLER void power_handler(interrupt_frame_t *frame) {}

void enable_acpi() {
    int i;
    if (io_in16(facp->pm1a_cnt_blk) & SCI_EN) {
        kinfo("ACPI already enabled.");
        return;
    }
    ioapic_add(power, facp->sci_int);
    register_interrupt_handler(power, power_handler, 0, 0x8E);
    if (facp->smi_cmd && facp->acpi_enable) {
        io_out8(facp->smi_cmd, facp->acpi_enable);
        for (i = 0; i < 300; i++) {
            if (io_in16(facp->pm1a_cnt_blk) & SCI_EN) break;
            nsleep(5);
        }
        if (facp->pm1b_cnt_blk) {
            for (int i = 0; i < 300; i++) {
                if (io_in16(facp->pm1b_cnt_blk) & SCI_EN) break;
                nsleep(5);
            }
        }
        if (i < 300)
            return;
        else
            kwarn("ACPI enable failed.");
    }
}

void setup_facp(acpi_facp_t *facp0) {
    uint8_t *S5Addr;
    uint32_t dsdtlen;
    facp = facp0;

    dsdt_table_t *dsdtTable = (dsdt_table_t *)(uint64_t)facp->dsdt;
    if (dsdtTable == NULL) {
        kwarn("Cannot find acpi dsdt table.");
        return;
    } else
        dsdtTable = phys_to_virt((uint64_t)dsdtTable);

    if (!memcmp(dsdtTable->signature, "DSDT", 4)) {
        S5Addr  = &(dsdtTable->definition_block);
        dsdtlen = dsdtTable->length - 36;

        while (dsdtlen--) {
            if (!memcmp(S5Addr, "_S5_", 4)) { break; }
            S5Addr++;
        }

        SLP_EN = 1 << 13;
        SCI_EN = 1;

        if (dsdtlen) {
            // check valid \_S5
            if (*(S5Addr - 1) == 0x08 || (*(S5Addr - 2) == 0x08 && *(S5Addr - 1) == '\\')) {
                S5Addr += 5;
                S5Addr += ((*S5Addr & 0xC0) >> 6) + 2;

                if (*S5Addr == 0x0A) { S5Addr++; }
                SLP_TYPa = *(S5Addr) << 10;
                S5Addr++;
                if (*S5Addr == 0x0A) { S5Addr++; }
                SLP_TYPb = *(S5Addr) << 10;
                S5Addr++;
            } else {
                kwarn("\\_S5 parse error!\n");
            }
        } else {
            kwarn("\\_S5 not present!\n");
        }
    } else
        kwarn("Cannot find acpi dsdt table.");

    enable_acpi();
}

void power_reset() {
    // uint8_t val;
    if (!SCI_EN) return;
    loop {
        // write ICH port
        io_out8(0x92, 0x01);
        // send RESET_VAL
        io_out8((uint32_t)facp->reset_reg.address, facp->reset_value);
    }
}

void power_off() {
    if (!SCI_EN) return;
    loop {
        io_out16((uint32_t)facp->pm1a_cnt_blk, SLP_TYPa | SLP_EN);
        if (facp->pm1b_cnt_blk) { io_out16((uint32_t)facp->pm1b_cnt_blk, SLP_TYPb | SLP_EN); }
    }
}
