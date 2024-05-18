#include "../include/ide.h"
#include "../include/printf.h"
#include "../include/common.h"
#include "../include/vdisk.h"
#include "../include/timer.h"
#include "../include/pci.h"

static inline nul(char *f, ...) {}

#define logk nul
#define inb io_in8
#define outb io_out8

struct IDEChannelRegisters {
    unsigned short base;  // I/O Base.
    unsigned short ctrl;  // Control Base
    unsigned short bmide; // Bus Master IDE
    unsigned char nIEN;   // nIEN (No Interrupt);
} channels[2];

struct ide_device {
    unsigned char Reserved;      // 0 (Empty) or 1 (This Drive really exists).
    unsigned char Channel;       // 0 (Primary Channel) or 1 (Secondary Channel).
    unsigned char Drive;         // 0 (Master Drive) or 1 (Slave Drive).
    unsigned short Type;         // 0: ATA, 1:ATAPI.
    unsigned short Signature;    // Drive Signature
    unsigned short Capabilities; // Features.
    unsigned int CommandSets;    // Command Sets Supported.
    unsigned int Size;           // Size in Sectors.
    unsigned char Model[41];     // Model in string.
} ide_devices[4];

unsigned char ide_buf[2048] = {0};
volatile unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int package[2];

static inline void insl(uint32_t port, uint32_t *addr, int cnt) {
    asm volatile("cld;"
                 "repne; insl;"
            : "=D"(addr), "=c"(cnt)
            : "d"(port), "0"(addr), "1"(cnt)
            : "memory", "cc");
}

static void Read(char drive, unsigned char *buffer, unsigned int number,
                 unsigned int lba) {
    ide_read_sectors(drive - 'A', number, lba, 1 * 8, buffer);
}

static void Write(char drive, unsigned char *buffer, unsigned int number,
                  unsigned int lba) {
    ide_write_sectors(drive - 'A', number, lba, 1 * 8, buffer);
}

void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2,
                    unsigned int BAR3, unsigned int BAR4) {
    int j, k, count = 0;
    for (int i = 0; i < 4; i++) {
        ide_devices[i].Reserved = 0;
    }
    // 1- Detect I/O Ports which interface IDE Controller:
    channels[ATA_PRIMARY].base = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
    channels[ATA_PRIMARY].ctrl = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
    channels[ATA_SECONDARY].base = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
    channels[ATA_SECONDARY].ctrl = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
    channels[ATA_PRIMARY].bmide = (BAR4 & 0xFFFFFFFC) + 0;   // Bus Master IDE
    channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE
    // 2- Disable IRQs:
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);
    logk("w1\n");
    // 3- Detect ATA-ATAPI Devices:
    for (int i = 0; i < 2; i++)
        for (j = 0; j < 2; j++) {
            unsigned char err = 0, type = IDE_ATA, status;
            ide_devices[count].Reserved = 0; // Assuming that no drive here.

            // (I) Select Drive:
            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
            sleep(1); // Wait 1ms for drive select to work.
            logk("I\n");
            // (II) Send ATA Identify Command:
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            sleep(1); // This function should be implemented in your OS. which waits
            // for 1 ms. it is based on System Timer Device Driver.
            logk("II\n");
            // (III) Polling:
            if (ide_read(i, ATA_REG_STATUS) == 0)
                continue; // If Status = 0, No Device.
            logk("III\n");
            while (1) {
                logk("read ide....\n");
                status = ide_read(i, ATA_REG_STATUS);
                logk("read ok\n");
                if ((status & ATA_SR_ERR)) {
                    err = 1;
                    break;
                } // If Err, Device is not ATA.
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                    break; // Everything is right.
            }

            // (IV) Probe for ATAPI Devices:
            logk("IV\n");
            if (err != 0) {
                unsigned char cl = ide_read(i, ATA_REG_LBA1);
                unsigned char ch = ide_read(i, ATA_REG_LBA2);

                if (cl == 0x14 && ch == 0xEB)
                    type = IDE_ATAPI;
                else if (cl == 0x69 && ch == 0x96)
                    type = IDE_ATAPI;
                else
                    continue; // Unknown Type (may not be a device).

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                sleep(1);
            }

            // (V) Read Identification Space of the Device:
            logk("V\n");
            ide_read_buffer(i, ATA_REG_DATA, (unsigned int) ide_buf, 128);

            // (VI) Read Device Parameters:
            logk("VI\n");
            ide_devices[count].Reserved = 1;
            ide_devices[count].Type = type;
            ide_devices[count].Channel = i;
            ide_devices[count].Drive = j;
            ide_devices[count].Signature =
                    *((unsigned short *) (ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities =
                    *((unsigned short *) (ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets =
                    *((unsigned int *) (ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size:
            logk("VII\n");
            if (ide_devices[count].CommandSets & (1 << 26))
                // Device uses 48-Bit Addressing:
                ide_devices[count].Size =
                        *((unsigned int *) (ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else
                // Device uses CHS or 28-bit Addressing:
                ide_devices[count].Size =
                        *((unsigned int *) (ide_buf + ATA_IDENT_MAX_LBA));

            // (VIII) String indicates model of device (like Western Digital HDD and
            // SONY DVD-RW...):
            logk("VIII\n");
            for (k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].Model[40] = 0; // Terminate String.

            count++;
        }

    // 4- Print Summary:
    vdisk vd;
    for (int i = 0; i < 4; i++)
        if (ide_devices[i].Reserved == 1) {
            logk(" %d Found %s Drive %dMB - %s\n", i,
                 (const char *[]) {"ATA", "ATAPI"}[ide_devices[i].Type], /* Type */
                    ide_devices[i].Size / 1024 / 2,                        /* Size */
                    ide_devices[i].Model);
            strcpy(vd.DriveName, ide_devices[i].Model);
            vd.flag = 1;
            vd.Read = Read;
            vd.Write = Write;
            vd.size = ide_devices[i].Size;
            printf("[Disk-(%c)]: Size: %dMB | Name: %s\n", register_vdisk(vd), vd.size, vd.DriveName);
        }
}

unsigned char ide_read(unsigned char channel, unsigned char reg) {
    unsigned char result;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        result = inb(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(channels[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(channels[channel].ctrl + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        outb(channels[channel].base + reg - 0x00, data);
    else if (reg < 0x0C)
        outb(channels[channel].base + reg - 0x06, data);
    else if (reg < 0x0E)
        outb(channels[channel].ctrl + reg - 0x0A, data);
    else if (reg < 0x16)
        outb(channels[channel].bmide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_read_buffer(unsigned char channel, unsigned char reg,
                     unsigned int buffer, unsigned int quads) {
    /* WARNING: This code contains a serious bug. The inline assembly trashes ES
     * and ESP for all of the code the compiler generates between the inline
     *           assembly blocks.
     */
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    // asm("pushw %es; movw %ds, %ax; movw %ax, %es");
    if (reg < 0x08)
        insl(channels[channel].base + reg - 0x00, buffer, quads);
    else if (reg < 0x0C)
        insl(channels[channel].base + reg - 0x06, buffer, quads);
    else if (reg < 0x0E)
        insl(channels[channel].ctrl + reg - 0x0A, buffer, quads);
    else if (reg < 0x16)
        insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
    // asm("popw %es;");
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

unsigned char ide_polling(unsigned char channel, unsigned int advanced_check) {
    // (I) Delay 400 nanosecond for BSY to be set:
    // -------------------------------------------------
    for (int i = 0; i < 4; i++)
        ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port
    // wastes 100ns; loop four times.

    // (II) Wait for BSY to be cleared:
    // -------------------------------------------------
    logk("II\n");
    int a = ide_read(channel, ATA_REG_STATUS);
    while (a & ATA_SR_BSY) {
        logk("a=%d\n", a & ATA_SR_BSY); // Wait for BSY to be zero.
        a = ide_read(channel, ATA_REG_STATUS);
        sleep(1);
    }
    logk("II OK\n");
    if (advanced_check) {
        unsigned char state =
                ide_read(channel, ATA_REG_STATUS); // Read Status Register.

        // (III) Check For Errors:
        // -------------------------------------------------
        logk("III\n");
        if (state & ATA_SR_ERR)
            return 2; // Error.

        // (IV) Check If Device fault:
        // -------------------------------------------------
        if (state & ATA_SR_DF)
            return 1; // Device Fault.

        // (V) Check DRQ:
        // -------------------------------------------------
        // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
        if ((state & ATA_SR_DRQ) == 0)
            return 3; // DRQ should be set
    }

    return 0; // No Error.
}

unsigned char ide_print_error(unsigned int drive, unsigned char err) {
    if (err == 0)
        return err;

    printk("IDE:");
    if (err == 1) {
        printk("- Device Fault\n     ");
        err = 19;
    } else if (err == 2) {
        unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
        if (st & ATA_ER_AMNF) {
            printk("- No Address Mark Found\n     ");
            err = 7;
        }
        if (st & ATA_ER_TK0NF) {
            printk("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_ABRT) {
            printk("- Command Aborted\n     ");
            err = 20;
        }
        if (st & ATA_ER_MCR) {
            printk("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_IDNF) {
            printk("- ID mark not Found\n     ");
            err = 21;
        }
        if (st & ATA_ER_MC) {
            printk("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_UNC) {
            printk("- Uncorrectable Data Error\n     ");
            err = 22;
        }
        if (st & ATA_ER_BBK) {
            printk("- Bad Sectors\n     ");
            err = 13;
        }
    } else if (err == 3) {
        printk("- Reads Nothing\n     ");
        err = 23;
    } else if (err == 4) {
        printk("- Write Protected\n     ");
        err = 8;
    }
    printk(
            "- [%s %s] %s\n",
            (const char *[]) {
                    "Primary",
                    "Secondary"}[ide_devices[drive].Channel], // Use the channel as an
            // index into the array
            (const char *[]) {
                    "Master",
                    "Slave"}[ide_devices[drive].Drive], // Same as above, using the drive
    ide_devices[drive].Model);

    return err;
}

unsigned char ide_ata_access(unsigned char direction, unsigned char drive,
                             unsigned int lba, unsigned char numsects,
                             unsigned short selector, unsigned int edi) {
    unsigned char lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */,
            dma /* 0: No DMA, 1: DMA */, cmd;
    unsigned char lba_io[6];
    unsigned int channel = ide_devices[drive].Channel; // Read the Channel.
    unsigned int slavebit =
            ide_devices[drive].Drive; // Read the Drive [Master/Slave]
    unsigned int bus =
            channels[channel].base; // Bus Base, like 0x1F0 which is also data port.
    unsigned int words =
            256; // Almost every ATA drive has a sector-size of 512-byte.
    unsigned short cyl, i;
    unsigned char head, sect, err;
    ide_write(channel, ATA_REG_CONTROL,
              channels[channel].nIEN = (ide_irq_invoked = 0x0) + 0x02);
    // (I) Select one from LBA28, LBA48 or CHS;
    logk("I %02x\n", channels[channel].nIEN);
    if (lba >= 0x10000000) { // Sure Drive should support LBA in this case, or
        // you are giving a wrong LBA.
        // LBA48:
        lba_mode = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        head = 0;      // Lower 4-bits of HDDEVSEL are not used here.
    } else if (ide_devices[drive].Capabilities & 0x200) { // Drive supports LBA?
        // LBA28:
        lba_mode = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0; // These Registers are not used here.
        lba_io[4] = 0; // These Registers are not used here.
        lba_io[5] = 0; // These Registers are not used here.
        head = (lba & 0xF000000) >> 24;
    } else {
        // CHS:
        lba_mode = 0;
        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba + 1 - sect) % (16 * 63) /
               (63); // Head number is written to HDDEVSEL lower 4-bits.
    }
    // (II) See if drive supports DMA or not;
    logk("II\n");
    dma = 0; // We don't support DMA
    // (III) Wait if the drive is busy;
    logk("III\n");
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {
    } // Wait if busy.
    // (IV) Select Drive from the controller;
    logk("IV\n");
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
    if (lba_mode == 0 && dma == 0 && direction == 0)
        cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0)
        cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 0)
        cmd = ATA_CMD_READ_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 0)
        cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0)
        cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0)
        cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1)
        cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1)
        cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1)
        cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1)
        cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1)
        cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1)
        cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write(channel, ATA_REG_COMMAND, cmd); // Send the Command.
    logk("IV1\n");
    if (dma)
        if (direction == 0);
            // DMA Read.
        else;
        // DMA Write.
    else if (direction == 0) {
        // PIO Read.
        uint16_t *word_ = edi;
        for (i = 0; i < numsects; i++) {
           // printf("read %d\n", i);
            if (err = ide_polling(channel, 1))
                return err; // Polling, set error and exit if there is.

            logk("words=%d bus=%d\n", words, bus);
            for (int h = 0; h < words; h++) {
              unsigned short a = io_in16(bus);
               word_[i * words + h] = a;
              // printf("%04x ",a);
             }
            //insl(bus, word_ + i * words, words / 2);
        }
    } else {
        // PIO Write.
        uint16_t *word_ = edi;
        for (i = 0; i < numsects; i++) {
            logk("write %d\n", i);
            ide_polling(channel, 0); // Polling.
            // asm("pushw %ds");
            // asm("mov %%ax, %%ds" ::"a"(selector));
            // asm("rep outsw" ::"c"(words), "d"(bus), "S"(edi));  // Send Data
            // asm("popw %ds");
            for (int h = 0; h < words; h++) {
                io_out16(bus, word_[i * words + h]);
            }
        }
        ide_write(channel, ATA_REG_COMMAND,
                  (char[]) {ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH,
                            ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
        ide_polling(channel, 0); // Polling.
    }

    return 0; // Easy, isn't it?
}

void ide_wait_irq() {
    while (!ide_irq_invoked);
    ide_irq_invoked = 0;
}

void ide_irq() {
    logk("ide irq.\n");
    ide_irq_invoked = 1;
}

unsigned char ide_atapi_read(unsigned char drive, unsigned int lba,
                             unsigned char numsects, unsigned short selector,
                             unsigned int edi) {
    logk("cdrom read.\n");
    unsigned int channel = ide_devices[drive].Channel;
    unsigned int slavebit = ide_devices[drive].Drive;
    unsigned int bus = channels[channel].base;
    unsigned int words =
            1024; // Sector Size. ATAPI drives have a sector size of 2048 bytes.
    unsigned char err;
    int i;

    ide_write(channel, ATA_REG_CONTROL,
              channels[channel].nIEN = ide_irq_invoked = 0x0);
    // (I): Setup SCSI Packet:
    // ------------------------------------------------------------------
    logk("I\n");
    atapi_packet[0] = ATAPI_CMD_READ;
    atapi_packet[1] = 0x0;
    atapi_packet[2] = (lba >> 24) & 0xFF;
    atapi_packet[3] = (lba >> 16) & 0xFF;
    atapi_packet[4] = (lba >> 8) & 0xFF;
    atapi_packet[5] = (lba >> 0) & 0xFF;
    atapi_packet[6] = 0x0;
    atapi_packet[7] = 0x0;
    atapi_packet[8] = 0x0;
    atapi_packet[9] = numsects;
    atapi_packet[10] = 0x0;
    atapi_packet[11] = 0x0; // (II): Select the drive:
    // ------------------------------------------------------------------
    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    logk("II\n");
    // (III): Delay 400 nanoseconds for select to complete:
    for (int i = 0; i < 4000; i++);
    logk("III\n");
    // ------------------------------------------------------------------
    logk("IV\n");
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
    logk("V\n");
    ide_write(channel, ATA_REG_LBA1,
              (words * 2) & 0xFF); // Lower Byte of Sector Size.
    ide_write(channel, ATA_REG_LBA2,
              (words * 2) >> 8); // Upper Byte of Sector Size.
    // (VI): Send the Packet Command:
    // ------------------------------------------------------------------
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET); // Send the Command.

    // (VII): Waiting for the driver to finish or return an error code:
    // ------------------------------------------------------------------
    if (err = ide_polling(channel, 1))
        return err; // Polling and return if error.

    // (VIII): Sending the packet data:
    // ------------------------------------------------------------------
    logk("VIII\n");
    uint16_t *_atapi_packet = atapi_packet;
    for (int i = 0; i < 6; i++) {
        io_out16(bus, _atapi_packet[i]);
    }
    // (IX): Receiving Data:
    // ------------------------------------------------------------------
    logk("IX\n");
    uint16_t *_word = edi;
    for (i = 0; i < numsects; i++) {
        ide_wait_irq(); // Wait for an IRQ.
        if (err = ide_polling(channel, 1))
            return err; // Polling and return if error.
        logk("words = %d\n", words);
        for (int h = 0; h < words; h++) {
            uint16_t a = io_in16(bus);
            _word[i * words + h] = a;
        }
    }
    // (X): Waiting for an IRQ:
    // ------------------------------------------------------------------
    ide_wait_irq();

    // (XI): Waiting for BSY & DRQ to clear:
    // ------------------------------------------------------------------
    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    return 0; // Easy, ... Isn't it?
}

void ide_read_sectors(unsigned char drive, unsigned char numsects,
                      unsigned int lba, unsigned short es, unsigned int edi) {
    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        package[0] = 0x1; // Drive Not Found!

        // 2: Check if inputs are valid:
        // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) &&
             (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2; // Seeking to invalid position.

        // 3: Read in PIO Mode through Polling & IRQs:
        // ============================================
    else {
        unsigned char err;
        if (ide_devices[drive].Type == IDE_ATA) {
            logk("Will Read.\n");
            err = ide_ata_access(ATA_READ, drive, lba, numsects, es, edi);

        } else if (ide_devices[drive].Type == IDE_ATAPI)
            for (int i = 0; i < numsects; i++)
                err = ide_atapi_read(drive, lba + i, 1, es, edi + (i * 2048));
        package[0] = ide_print_error(drive, err);
    }
}

void ide_write_sectors(unsigned char drive, unsigned char numsects,
                       unsigned int lba, unsigned short es, unsigned int edi) {
    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        package[0] = 0x1; // Drive Not Found!
        // 2: Check if inputs are valid:
        // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) &&
             (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2; // Seeking to invalid position.
        // 3: Read in PIO Mode through Polling & IRQs:
        // ============================================
    else {
        unsigned char err;
        if (ide_devices[drive].Type == IDE_ATA)
            err = ide_ata_access(ATA_WRITE, drive, lba, numsects, es, edi);
        else if (ide_devices[drive].Type == IDE_ATAPI)
            err = 4; // Write-Protected.
        package[0] = ide_print_error(drive, err);
    }
}

void ide_atapi_eject(unsigned char drive) {
    unsigned int channel = ide_devices[drive].Channel;
    unsigned int slavebit = ide_devices[drive].Drive;
    unsigned int bus = channels[channel].base;
    unsigned int words = 2048 / 2; // Sector Size in Words.
    unsigned char err = 0;
    ide_irq_invoked = 0;

    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        package[0] = 0x1; // Drive Not Found!
        // 2: Check if drive isn't ATAPI:
        // ==================================
    else if (ide_devices[drive].Type == IDE_ATA)
        package[0] = 20; // Command Aborted.
        // 3: Eject ATAPI Driver:
        // ============================================
    else {
        // Enable IRQs:
        ide_write(channel, ATA_REG_CONTROL,
                  channels[channel].nIEN = ide_irq_invoked = 0x0);

        // (I): Setup SCSI Packet:
        // ------------------------------------------------------------------
        atapi_packet[0] = ATAPI_CMD_EJECT;
        atapi_packet[1] = 0x00;
        atapi_packet[2] = 0x00;
        atapi_packet[3] = 0x00;
        atapi_packet[4] = 0x02;
        atapi_packet[5] = 0x00;
        atapi_packet[6] = 0x00;
        atapi_packet[7] = 0x00;
        atapi_packet[8] = 0x00;
        atapi_packet[9] = 0x00;
        atapi_packet[10] = 0x00;
        atapi_packet[11] = 0x00;

        // (II): Select the Drive:
        // ------------------------------------------------------------------
        ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);

        // (III): Delay 400 nanosecond for select to complete:
        // ------------------------------------------------------------------
        for (int i = 0; i < 4; i++)
            ide_read(
                    channel,
                    ATA_REG_ALTSTATUS); // Reading Alternate Status Port wastes 100ns.

        // (IV): Send the Packet Command:
        // ------------------------------------------------------------------
        ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET); // Send the Command.

        // (V): Waiting for the driver to finish or invoke an error:
        // ------------------------------------------------------------------
        err = ide_polling(channel, 1); // Polling and stop if error.

        // (VI): Sending the packet data:
        // ------------------------------------------------------------------

        asm("rep   outsw"::"c"(6), "d"(bus),
        "S"(atapi_packet));        // Send Packet Data
        ide_wait_irq();                // Wait for an IRQ.
        err = ide_polling(channel, 1); // Polling and get error code.
        if (err == 3)
            err = 0; // DRQ is not needed here.

        package[0] = ide_print_error(drive, err); // Return;
    }
}