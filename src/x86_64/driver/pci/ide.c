#include "ide.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "krlibc.h"
#include "pci.h"
#include "pcie.h"
#include "sprintf.h"
#include "timer.h"
#include "vdisk.h"

#define klog(...)                                                                                  \
    do {                                                                                           \
    } while (0);
#define error(...)                                                                                 \
    do {                                                                                           \
    } while (0);

struct IDEChannelRegisters channels[2];
struct ide_device          ide_devices[4];
uint8_t                    ide_buf[2048]    = {0};
volatile static uint8_t    ide_irq_invoked  = 0;
static uint8_t             atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int                 package[2];
static int                 drive_mapping[26] = {0};

void ide_wait_irq() {
    while (!ide_irq_invoked)
        ;
    ide_irq_invoked = 0;
}

uint8_t ide_print_error(uint32_t drive, uint8_t err) {
    if (err == 0) return err;

    error("IDE:");
    if (err == 1) {
        error("- Device Fault\n     ");
        err = 19;
    } else if (err == 2) {
        uint8_t st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
        if (st & ATA_ER_AMNF) {
            error("- No Address Mark Found\n     ");
            err = 7;
        }
        if (st & ATA_ER_TK0NF) {
            error("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_ABRT) {
            error("- Command Aborted\n     ");
            err = 20;
        }
        if (st & ATA_ER_MCR) {
            error("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_IDNF) {
            error("- ID mark not Found\n     ");
            err = 21;
        }
        if (st & ATA_ER_MC) {
            error("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_UNC) {
            error("- Uncorrectable Data Error\n     ");
            err = 22;
        }
        if (st & ATA_ER_BBK) {
            error("- Bad Sectors\n     ");
            err = 13;
        }
    } else if (err == 3) {
        error("- Reads Nothing\n     ");
        err = 23;
    } else if (err == 4) {
        error("- Write Protected\n     ");
        err = 8;
    }
    error("- [%s %s] %s\n",
          (const char *[]){"Primary",
                           "Secondary"}[ide_devices[drive].Channel], // Use the channel as an
          // index into the array
          (const char *[]){"Master",
                           "Slave"}[ide_devices[drive].Drive], // Same as above, using the drive
          ide_devices[drive].Model);

    return err;
}

uint8_t ide_polling(uint8_t channel, uint32_t advanced_check) {
    // (I) Delay 400 nanosecond for BSY to be set:
    // -------------------------------------------------
    for (int i = 0; i < 4; i++)
        ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port
    // wastes 100ns; loop four times.

    // (II) Wait for BSY to be cleared:
    // -------------------------------------------------
    klog("II");
    int a = ide_read(channel, ATA_REG_STATUS);
    while (a & ATA_SR_BSY) {
        klog("a=%d\n", a & ATA_SR_BSY); // Wait for BSY to be zero.
        a = ide_read(channel, ATA_REG_STATUS);
        usleep(10);
    }
    klog("II OK");
    if (advanced_check) {
        uint8_t state = ide_read(channel, ATA_REG_STATUS); // Read Status Register.

        // (III) Check For Errors:
        // -------------------------------------------------
        klog("III");
        if (state & ATA_SR_ERR) return 2; // Error.

        // (IV) Check If Device fault:
        // -------------------------------------------------
        if (state & ATA_SR_DF) return 1; // Device Fault.

        // (V) Check DRQ:
        // -------------------------------------------------
        // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
        if ((state & ATA_SR_DRQ) == 0) return 3; // DRQ should be set
    }

    return 0; // No Error.
}

void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
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
    uint8_t result = 0;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        result = io_in8(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = io_in8(channels[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        result = io_in8(channels[channel].ctrl + reg - 0x0A);
    else
        result = io_in8(channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C) ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint64_t buffer, uint32_t quads) {
    /* WARNING: This code contains a serious bug. The inline assembly trashes ES
     * and ESP for all of the code the compiler generates between the inline
     *           assembly blocks.
     */
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
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

uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects,
                       uint16_t selector, uint64_t edi) {
    uint8_t  lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
    uint8_t  lba_io[6];
    uint8_t  channel  = ide_devices[drive].Channel; // Read the Channel.
    uint32_t slavebit = ide_devices[drive].Drive;   // Read the Drive [Master/Slave]
    uint32_t bus      = channels[channel].base;     // Bus Base, like 0x1F0 which is also data port.
    uint32_t words    = 256; // Almost every ATA drive has a sector-size of 512-byte.
    uint16_t cyl, i;
    uint8_t  head, sect, err;
    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = (ide_irq_invoked = 0x0) + 0x02);
    // (I) Select one from LBA28, LBA48 or CHS;
    klog("I %02x", channels[channel].nIEN);
    if (lba >= 0x10000000) { // Sure Drive should support LBA in this case, or
        // you are giving a wrong LBA.
        // LBA48:
        lba_mode  = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
    } else if (ide_devices[drive].Capabilities & 0x200) { // Drive supports LBA?
        // LBA28:
        lba_mode  = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0; // These Registers are not used here.
        lba_io[4] = 0; // These Registers are not used here.
        lba_io[5] = 0; // These Registers are not used here.
        head      = (lba & 0xF000000) >> 24;
    } else {
        // CHS:
        lba_mode  = 0;
        sect      = (lba % 63) + 1;
        cyl       = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head =
            (lba + 1 - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
    }
    // (II) See if drive supports DMA or not;
    klog("II");
    dma = 0; // We don't support DMA
    // (III) Wait if the drive is busy;
    klog("III");
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {} // Wait if busy.
    // (IV) Select Drive from the controller;
    klog("IV");
    if (lba_mode == 0)
        ide_write(channel, ATA_REG_HDDEVSEL,
                  0xA0 | (slavebit << 4) | head); // Drive & CHS.
    else
        ide_write(channel, ATA_REG_HDDEVSEL,
                  0xE0 | (slavebit << 4) | head); // Drive & LBA
    // (V) Write Parameters;
    if (lba_mode == 2) {
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_write(channel, ATA_REG_LBA3, lba_io[3]);
        ide_write(channel, ATA_REG_LBA4, lba_io[4]);
        ide_write(channel, ATA_REG_LBA5, lba_io[5]);
    }
    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);
    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write(channel, ATA_REG_COMMAND, cmd); // Send the Command.
    klog("IV1");
    if (dma)
        if (direction == 0)
            ;
        // DMA Read.
        else
            ;
    // DMA Write.
    else if (direction == 0) {
        // PIO Read.
        uint16_t *word_ = (uint16_t *)edi;
        for (i = 0; i < numsects; i++) {
            klog("read %d", i);
            if ((err = ide_polling(channel, 1)) != 0)
                return err; // Polling, set error and exit if there is.

            klog("words=%d bus=%d", words, bus);
            // for (int h = 0; h < words; h++) {
            //   uint16_t a = io_in16(bus);
            //   word_[i * words + h] = a;
            // }
            insl(bus, (uint32_t *)(word_ + i * words), words / 2);
        }
    } else {
        // PIO Write.
        uint16_t *word_ = (uint16_t *)edi;
        for (i = 0; i < numsects; i++) {
            klog("write %d", i);
            ide_polling(channel, 0); // Polling.
            // asm("pushw %ds");
            // asm("mov %%ax, %%ds" ::"a"(selector));
            // asm("rep outsw" ::"c"(words), "d"(bus), "S"(edi));  // Send Data
            // asm("popw %ds");
            for (int h = 0; h < words; h++) {
                io_out16(bus, word_[i * words + h]);
            }
        }
        ide_write(
            channel, ATA_REG_COMMAND,
            (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
        ide_polling(channel, 0); // Polling.
    }

    return 0; // Easy, isn't it?
}

uint8_t ide_atapi_read(uint8_t drive, uint32_t lba, uint8_t numsects, uint16_t selector,
                       uint32_t edi) {
    klog("cdrom read.");
    uint32_t channel  = ide_devices[drive].Channel;
    uint32_t slavebit = ide_devices[drive].Drive;
    uint32_t bus      = channels[channel].base;
    uint32_t words    = 1024; // Sector Size. ATAPI drives have a sector size of 2048 bytes.
    uint8_t  err;
    int      i;

    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0);

    // (I): Setup SCSI Packet:
    // ------------------------------------------------------------------
    klog("I");
    atapi_packet[0]  = ATAPI_CMD_READ;
    atapi_packet[1]  = 0x0;
    atapi_packet[2]  = (lba >> 24) & 0xFF;
    atapi_packet[3]  = (lba >> 16) & 0xFF;
    atapi_packet[4]  = (lba >> 8) & 0xFF;
    atapi_packet[5]  = (lba >> 0) & 0xFF;
    atapi_packet[6]  = 0x0;
    atapi_packet[7]  = 0x0;
    atapi_packet[8]  = 0x0;
    atapi_packet[9]  = numsects;
    atapi_packet[10] = 0x0;
    atapi_packet[11] = 0x0; // (II): Select the drive:
    // ------------------------------------------------------------------
    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    klog("II");
    // (III): Delay 400 nanoseconds for select to complete:
    for (int i = 0; i < 4000; i++)
        ;
    klog("III");
    // ------------------------------------------------------------------
    klog("IV");

    for (int i = 0; i < 4; i++)
        ide_read(channel,
                 ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes

    // 100ns. (IV): Inform the Controller that we
    // use PIO mode:
    // ------------------------------------------------------------------
    ide_write(channel, ATA_REG_FEATURES,
              0); // PIO mode.
    // (V): Tell the Controller the size of buffer:
    // ------------------------------------------------------------------
    klog("V");
    ide_write(channel, ATA_REG_LBA1,
              (words * 2) & 0xFF); // Lower Byte of Sector Size.
    ide_write(channel, ATA_REG_LBA2,
              (words * 2) >> 8); // Upper Byte of Sector Size.
    // (VI): Send the Packet Command:
    // ------------------------------------------------------------------
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET); // Send the Command.

    // (VII): Waiting for the driver to finish or return an error code:
    // ------------------------------------------------------------------

    if ((err = ide_polling(channel, 1)) != NULL) return err; // Polling and return if error.

    // (VIII): Sending the packet data:
    // ------------------------------------------------------------------
    klog("VIII");
    uint16_t *_atapi_packet = (uint16_t *)atapi_packet;
    for (int i = 0; i < 6; i++) {
        io_out16(bus, _atapi_packet[i]);
    }
    // (IX): Receiving Data:
    // ------------------------------------------------------------------
    klog("IX");

    uint16_t *_word = (uint16_t *)edi;
    for (i = 0; i < numsects; i++) {
        ide_wait_irq();                                          // Wait for an IRQ.
        if ((err = ide_polling(channel, 1)) != NULL) return err; // Polling and return if error.
        klog("words = %d\n", words);
        for (int h = 0; h < words; h++) {
            uint16_t a           = io_in16(bus);
            _word[i * words + h] = a;
        }
    }

    // (X): Waiting for BSY & DRQ to clear:
    // ------------------------------------------------------------------

    waitif(ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    return 0; // Easy, ... Isn't it?
}

void ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, uint16_t es, uint32_t edi) {
    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0) package[0] = 0x1; // Drive Not Found!

    // 2: Check if inputs are valid:
    // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2; // Seeking to invalid position.

    // 3: Read in PIO Mode through Polling & IRQs:
    // ============================================
    else {
        uint8_t err;
        if (ide_devices[drive].Type == IDE_ATA) {
            err = ide_ata_access(ATA_READ, drive, lba, numsects, es, edi);

        } else if (ide_devices[drive].Type == IDE_ATAPI)
            for (int i = 0; i < numsects; i++)
                err = ide_atapi_read(drive, lba + i, 1, es, edi + (i * 2048));
        package[0] = ide_print_error(drive, err);
    }
}
// package[0] is an entry of an array. It contains the Error Code.
void ide_write_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, uint16_t es, uint32_t edi) {
    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0) package[0] = 0x1; // Drive Not Found!
    // 2: Check if inputs are valid:
    // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2; // Seeking to invalid position.
    // 3: Read in PIO Mode through Polling & IRQs:
    // ============================================
    else {
        uint8_t err;
        if (ide_devices[drive].Type == IDE_ATA)
            err = ide_ata_access(ATA_WRITE, drive, lba, numsects, es, edi);
        else if (ide_devices[drive].Type == IDE_ATAPI)
            err = 4; // Write-Protected.
        package[0] = ide_print_error(drive, err);
    }
}

static void Read(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    ide_read_sectors(drive_mapping[drive], number, lba, 1 * 8, (uint32_t)buffer);
}
static void Write(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    ide_write_sectors(drive_mapping[drive], number, lba, 1 * 8, (uint32_t)buffer);
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
            usleep(10);                                      // Wait 1ms for drive select to work.
            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            usleep(10); // This function should be implemented in your OS. which waits
            // for 1 ms. it is based on System Timer Device Driver.
            // (III) Polling:
            if (ide_read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.
            infinite_loop {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR)) {
                    err = 1;
                    break;
                } // If Err, Device is not ATA.
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
    vdisk vd;
    for (int i = 0; i < 4; i++)
        if (ide_devices[i].Reserved == 1) {
            kinfo("IDE %d Found %s Drive %dMB - %s", i, ide_devices[i].Type ? "ATAPI" : "ATA",
                  ide_devices[i].Size / 1024 / 2, ide_devices[i].Model);
            sprintf(vd.drive_name, "ide%d", i);
            if (ide_devices[i].Type == IDE_ATAPI) {
                vd.flag = 2;
            } else {
                vd.flag = 1;
            }

            vd.read        = Read;
            vd.write       = Write;
            vd.size        = ide_devices[i].Size;
            vd.sector_size = vd.flag == 2 ? 2048 : 512;

            int c            = regist_vdisk(vd);
            drive_mapping[c] = i;
        }
}

__IRQHANDLER void ide_irq(interrupt_frame_t *reg) {
    ide_irq_invoked = 1;
}

void ide_setup() {
    pcie_device_t *pcie_d = pcie_find_class(0x10100);
    if (pcie_d == NULL) {
        pci_device_t pci_d = pci_find_class(0x10100);
        if (pci_d == NULL) return;
    }
    register_interrupt_handler(ide_primary, ide_irq, 0, 0x8E);
    register_interrupt_handler(ide_secondary, ide_irq, 0, 0x8E);
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    kinfo("IDE device load done!");
}
