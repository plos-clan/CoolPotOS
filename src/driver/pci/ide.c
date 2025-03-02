#include "ide.h"
#include "pcie.h"
#include "pci.h"
#include "io.h"
#include "timer.h"
#include "kprint.h"

struct IDEChannelRegisters channels[2];
struct ide_device ide_devices[4];
uint8_t ide_buf[2048]    = {0};
volatile static uint8_t ide_irq_invoked  = 0;
static uint8_t atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int package[2];

void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        io_out8(channels[channel].base + reg - 0x00, data);
    else if (reg < 0x0C)
        io_out8(channels[channel].base + reg - 0x06, data);
    else if (reg < 0x0E)
        io_out8(channels[channel].ctrl + reg - 0x0A, data);
    else if (reg < 0x16)
        io_out8(channels[channel].bmide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

uint8_t ide_read(uint8_t channel, uint8_t reg) {
    uint8_t result;
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        result = io_in8(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = io_in8(channels[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        result = io_in8(channels[channel].ctrl + reg - 0x0A);
    else if (reg < 0x16)
        result = io_in8(channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint64_t buffer, uint32_t quads) {
    /* WARNING: This code contains a serious bug. The inline assembly trashes ES
     * and ESP for all of the code the compiler generates between the inline
     *           assembly blocks.
     */
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    // asm("pushw %es; movw %ds, %ax; movw %ax, %es");
    if (reg < 0x08)
        insl(channels[channel].base + reg - 0x00, (uint32_t *)buffer, quads);
    else if (reg < 0x0C)
        insl(channels[channel].base + reg - 0x06, (uint32_t *)buffer, quads);
    else if (reg < 0x0E)
        insl(channels[channel].ctrl + reg - 0x0A, (uint32_t *)buffer, quads);
    else if (reg < 0x16)
        insl(channels[channel].bmide + reg - 0x0E, (uint32_t *)buffer, quads);
    // asm("popw %es;");
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_initialize(uint32_t BAR0, uint32_t BAR1, uint32_t BAR2, uint32_t BAR3, uint32_t BAR4) {
    int j, k, count = 0;
    for (int i = 0; i < 4; i++) {
        ide_devices[i].Reserved = 0;
    }
    // 1- Detect I/O Ports which interface IDE Controller:
    channels[ATA_PRIMARY].base    = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
    channels[ATA_PRIMARY].ctrl    = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
    channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
    channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
    channels[ATA_PRIMARY].bmide   = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
    channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE
    // 2- Disable IRQs:
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);
    for (int i = 0; i < 2; i++)
        for (j = 0; j < 2; j++) {
            uint8_t err = 0, type = IDE_ATA, status;
            ide_devices[count].Reserved = 0; // Assuming that no drive here.

            // (I) Select Drive:
            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
            usleep(10);                                        // Wait 1ms for drive select to work.
            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            usleep(10); // This function should be implemented in your OS. which waits
            // for 1 ms. it is based on System Timer Device Driver.
            // (III) Polling:
            if (ide_read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.
            while (1) {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR)) {
                    err = 1;
                    break;
                }                                                           // If Err, Device is not ATA.
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break; // Everything is right.
            }

            // (IV) Probe for ATAPI Devices:
            if (err != 0) {
                uint8_t cl = ide_read(i, ATA_REG_LBA1);
                uint8_t ch = ide_read(i, ATA_REG_LBA2);

                if (cl == 0x14 && ch == 0xEB)
                    type = IDE_ATAPI;
                else if (cl == 0x69 && ch == 0x96)
                    type = IDE_ATAPI;
                else
                    continue; // Unknown Type (may not be a device).

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                usleep(10);
            }

            // (V) Read Identification Space of the Device:
            ide_read_buffer(i, ATA_REG_DATA, (uint64_t)ide_buf, 128);

            // (VI) Read Device Parameters:
            ide_devices[count].Reserved     = 1;
            ide_devices[count].Type         = type;
            ide_devices[count].Channel      = i;
            ide_devices[count].Drive        = j;
            ide_devices[count].Signature    = *((uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets  = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size:
            if (ide_devices[count].CommandSets & (1 << 26))
                // Device uses 48-Bit Addressing:
                ide_devices[count].Size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else
                // Device uses CHS or 28-bit Addressing:
                ide_devices[count].Size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA));

            // (VIII) String indicates model of device (like Western Digital HDD and
            // SONY DVD-RW...):
            for (k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k]     = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].Model[40] = 0; // Terminate String.

            count++;
        }

}

void ide_setup(){
    pcie_device_t *pcie_d = pcie_find_class(0x10100);
    if(pcie_d == NULL){
        pci_device_t pci_d = pci_find_class(0x10100);
        if(pci_d == NULL) return;
    }
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    kinfo("IDE device load done!");
}

