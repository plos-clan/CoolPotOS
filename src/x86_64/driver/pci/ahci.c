#include "ahci.h"
#include "heap.h"
#include "hhdm.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "page.h"
#include "pci.h"
#include "sprintf.h"
#include "timer.h"
#include "vdisk.h"

typedef struct ahci_port_ary {
    uint32_t port;
    int      type;
} AHCI_PORT_ARY;

static uint32_t cdb_size[] = {SCSI_CDB12, SCSI_CDB16, 0, 0};

static AHCI_PORT_ARY ahci_ports[32];
static uint32_t      port_total = 0;
static uint64_t      ahci_ports_base_addr;
static uint32_t      drive_mapping[0xff] = {0};
HBA_MEM             *hba_mem;

static int check_type(HBA_PORT *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    while (port->cmd & HBA_PxCMD_CR)
        ;
    port->cmd |= HBA_PxCMD_FRE | HBA_PxCMD_ST;

    uint32_t ssts = port->ssts;
    uint8_t  ipm  = (ssts >> 8) & 0x0F;
    uint8_t  det  = ssts & 0x0F;
    if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;

    logkf("SATA drive found at port %d\n", port_total);

    switch (port->sig) {
    case SATA_SIG_ATAPI: return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB: return AHCI_DEV_SEMB;
    case SATA_SIG_PM: return AHCI_DEV_PM;
    default: return AHCI_DEV_SATA;
    }
}

static void sata_create_fis(FIS_REG_H2D *cmdfis, uint8_t command, uint64_t lba,
                            uint16_t sector_count) {
    memset(cmdfis, 0, sizeof(FIS_REG_H2D));
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c        = 1; // Command
    cmdfis->command  = command;
    cmdfis->device   = 0;

    cmdfis->lba0 = SATA_LBA_COMPONENT(lba, 0);
    cmdfis->lba1 = SATA_LBA_COMPONENT(lba, 8);
    cmdfis->lba2 = SATA_LBA_COMPONENT(lba, 16);
    cmdfis->lba3 = SATA_LBA_COMPONENT(lba, 24);

    cmdfis->lba4 = SATA_LBA_COMPONENT(lba, 32);
    cmdfis->lba5 = SATA_LBA_COMPONENT(lba, 40);

    cmdfis->countl = sector_count;
}

void scsi_create_packet12(SCSI_cdb12 *cdb, uint8_t opcode, uint32_t lba, uint32_t alloc_size) {
    memset(cdb, 0, sizeof(SCSI_cdb12));
    cdb->opcode = opcode;
    cdb->lba_be = SCSI_FLIP(lba);
    cdb->length = SCSI_FLIP(alloc_size);
}

void scsi_create_packet16(SCSI_cdb16 *cdb, uint8_t opcode, uint64_t lba, uint32_t alloc_size) {
    memset(cdb, 0, sizeof(SCSI_cdb16));
    cdb->opcode    = opcode;
    cdb->lba_be_hi = SCSI_FLIP((uint32_t)(lba >> 32));
    cdb->lba_be_lo = SCSI_FLIP((uint32_t)lba);
    cdb->length    = SCSI_FLIP(alloc_size);
}

static void start_cmd(HBA_PORT *port) {
    // Wait until CR (bit15) is cleared
    waitif(port->cmd & HBA_PxCMD_CR);

    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

// Stop command engine
static void stop_cmd(HBA_PORT *port) {
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;

    // Clear FRE (bit4)
    port->cmd &= ~HBA_PxCMD_FRE;

    // Wait until FR (bit14), CR (bit15) are cleared
    infinite_loop {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
}

static int find_cmdslot(HBA_PORT *port) {
    // If not set in SACT and CI, the slot is free
    uint32_t slots    = (port->sact | port->ci);
    int      cmdslots = (hba_mem->cap & 0x1f00) >> 8;
    for (int i = 0; i < cmdslots; i++) {
        if ((slots & 1) == 0) return i;
        slots >>= 1;
    }
    kwarn("Cannot find free command list entry");
    return -1;
}

void ahci_port_rebase(HBA_PORT *port, int portno) {
    stop_cmd(port); // Stop command engine

    // Command list offset: 1K*portno
    // Command list entry size = 32
    // Command list entry maxim count = 32
    // Command list maxim size = 32*32 = 1K per port
    uint64_t phy_clb = ((uint64_t)virt_to_phys(ahci_ports_base_addr)) + (portno << 10);
    port->clb        = phy_clb & 0xFFFFFFFF;
    port->clbu       = phy_clb >> 32;
    memset(phys_to_virt(phy_clb), 0, 1024);

    // FIS offset: 32K+256*portno
    // FIS entry size = 256 bytes per port
    uint64_t phy_fb = ((uint64_t)virt_to_phys(ahci_ports_base_addr)) + (32 << 10) + (portno << 8);
    port->fb        = phy_fb & 0xFFFFFFFF;
    port->fbu       = phy_fb >> 32;
    memset(phys_to_virt(phy_fb), 0, 256);

    // Command table offset: 40K + 8K*portno
    // Command table size = 256*32 = 8K per port
    uint64_t        phy_cmd_handler = ((uint64_t)port->clbu << 32) | port->clb;
    HBA_CMD_HEADER *cmdheader       = (HBA_CMD_HEADER *)phys_to_virt(phy_cmd_handler);
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8; // 8 prdt entries per command table
        // 256 bytes per command table, 64+16+48+16*8
        // Command table offset: 40K + 8K*portno + cmdheader_index*256
        uint64_t phy_ctba =
            ((uint64_t)virt_to_phys(ahci_ports_base_addr)) + (40 << 10) + (portno << 13) + (i << 8);
        cmdheader[i].ctba  = phy_ctba & 0xFFFFFFFF;
        cmdheader[i].ctbau = phy_ctba >> 32;
        memset(phys_to_virt(cmdheader[i].ctba), 0, 256);
    }

    start_cmd(port); // Start command engine
}

bool ahci_identify(HBA_PORT *port, void *buf, int type) {
    port->is = (uint32_t)-1; // Clear pending interrupt bits
    int spin = 0;            // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    uint64_t        phy_cmd_handler  = ((uint64_t)port->clbu << 32) | port->clb;
    HBA_CMD_HEADER *cmdheader        = (HBA_CMD_HEADER *)phys_to_virt(phy_cmd_handler);
    cmdheader                       += slot;
    cmdheader->cfl                   = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
    cmdheader->w                     = 0;                                      // Read from device
    cmdheader->prdtl                 = 1;                                      // PRDT entries count
    cmdheader->c                     = 1;

    uint64_t     phy_cmd_tbl_handler = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
    HBA_CMD_TBL *cmdtbl              = (HBA_CMD_TBL *)phys_to_virt(phy_cmd_tbl_handler);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));
    uint64_t phys_buf          = (uint64_t)virt_to_phys((uint64_t)buf);
    cmdtbl->prdt_entry[0].dba  = phys_buf & 0xFFFFFFFF;
    cmdtbl->prdt_entry[0].dbau = phys_buf >> 32;
    cmdtbl->prdt_entry[0].dbc  = 0x200 - 1;
    cmdtbl->prdt_entry[0].i    = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);
    sata_create_fis(cmdfis, type == AHCI_DEV_SATAPI ? 0xa1 : (type == AHCI_DEV_SATA ? 0xec : 0), 0,
                    0);

    // The below loop waits until the port is no longer busy before issuing a new
    // command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == MAX_WAIT_INDEX) {
        kwarn("Port is hung");
        return false;
    }

    port->ci = 1 << slot; // Issue command

    // Wait for completion
    infinite_loop {
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) // Task file error
        {
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES) { return false; }

    if (type == AHCI_DEV_SATAPI) return true;
    sata_create_fis(cmdfis, ATA_PACKET, 512 << 8, 0);

    SATA_ident_t *buf_ident = (SATA_ident_t *)buf;
    uint32_t      cdb_size0 = cdb_size[buf_ident->config & 0x3];
    if (cdb_size0 == SCSI_CDB12) {
        SCSI_cdb12 *cdb12 = (SCSI_cdb12 *)cmdtbl->acmd;
        scsi_create_packet12(cdb12, SCSI_READ_CAPACITY_10, 0, 512 << 8);
    } else {
        SCSI_cdb16 *cdb16 = (SCSI_cdb16 *)cmdtbl->acmd;
        scsi_create_packet16(cdb16, SCSI_READ_CAPACITY_16, 0, 512);
        cdb16->misc1 = 0x10; // service action
    }

    cmdheader->prdbc = 0;
    cmdheader->a     = 1;

    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == MAX_WAIT_INDEX) {
        kwarn("SATAPI port is hung");
        return false;
    }

    return true;
}

bool ahci_write(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t)-1; // Clear pending interrupt bits
    int spin = 0;            // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    uint64_t        phy_cmd_handler  = ((uint64_t)port->clbu << 32) | port->clb;
    HBA_CMD_HEADER *cmdheader        = (HBA_CMD_HEADER *)phys_to_virt(phy_cmd_handler);
    cmdheader                       += slot;
    cmdheader->cfl                   = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
    cmdheader->w                     = 1;                                      // 写硬盘
    cmdheader->p                     = 1;
    cmdheader->c                     = 1;
    cmdheader->prdtl                 = (uint16_t)((count - 1) >> 4) + 1; // PRDT entries count

    uint64_t     phy_cmd_tbl_handler = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
    HBA_CMD_TBL *cmdtbl              = (HBA_CMD_TBL *)phys_to_virt(phy_cmd_tbl_handler);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    // 8K bytes (16 sectors) per PRDT
    int i;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        uint64_t phys_buf          = (uint64_t)virt_to_phys((uint64_t)buf);
        cmdtbl->prdt_entry[i].dba  = phys_buf & 0xFFFFFFFF;
        cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
        cmdtbl->prdt_entry[i].dbc =
            8 * 1024 - 1; // 8K bytes (this value should always be set to 1 less
        // than the actual value)
        cmdtbl->prdt_entry[i].i  = 1;
        buf                     += 4 * 1024; // 4K words
        count                   -= 16;       // 16 sectors
    }
    // Last entry
    uint64_t phys_buf          = (uint64_t)virt_to_phys((uint64_t)buf);
    cmdtbl->prdt_entry[i].dba  = phys_buf & 0xFFFFFFFF;
    cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
    cmdtbl->prdt_entry[i].dbc  = (count << 9) - 1; // 512 bytes per sector
    cmdtbl->prdt_entry[i].i    = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c        = 1; // Command
    cmdfis->command  = AHCI_CMD_WRITE_DMA_EXT;

    cmdfis->lba0   = (uint8_t)startl;
    cmdfis->lba1   = (uint8_t)(startl >> 8);
    cmdfis->lba2   = (uint8_t)(startl >> 16);
    cmdfis->device = 1 << 6; // LBA mode

    cmdfis->lba3 = (uint8_t)(startl >> 24);
    cmdfis->lba4 = (uint8_t)starth;
    cmdfis->lba5 = (uint8_t)(starth >> 8);

    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    // The below loop waits until the port is no longer busy before issuing a new
    // command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        logk("Port is hung\n");
        return false;
    }

    port->ci = 1 << slot; // Issue command

    // Wait for completion
    infinite_loop {
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) // Task file error
        {
            kwarn("Write ahci disk error\n");
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES) {
        kwarn("Write ahci disk error\n");
        return false;
    }

    return true;
}

bool ahci_read(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t)-1; // Clear pending interrupt bits
    int spin = 0;            // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    uint64_t        phy_cmd_handler  = ((uint64_t)port->clbu << 32) | port->clb;
    HBA_CMD_HEADER *cmdheader        = (HBA_CMD_HEADER *)phys_to_virt(phy_cmd_handler);
    cmdheader                       += slot;
    cmdheader->cfl                   = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
    cmdheader->w                     = 0;                                      // Read from device
    cmdheader->c                     = 1;
    cmdheader->p                     = 1;
    cmdheader->prdtl                 = (uint16_t)((count - 1) >> 4) + 1; // PRDT entries count

    uint64_t     phy_cmd_tbl_handler = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
    HBA_CMD_TBL *cmdtbl              = (HBA_CMD_TBL *)phys_to_virt(phy_cmd_tbl_handler);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    // 8K bytes (16 sectors) per PRDT
    int i;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        uint64_t phys_buf          = (uint64_t)virt_to_phys((uint64_t)buf);
        cmdtbl->prdt_entry[i].dba  = phys_buf & 0xFFFFFFFF;
        cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
        cmdtbl->prdt_entry[i].dbc =
            8 * 1024 - 1; // 8K bytes (this value should always be set to 1 less
        // than the actual value)
        cmdtbl->prdt_entry[i].i  = 1;
        buf                     += 4 * 1024; // 4K words
        count                   -= 16;       // 16 sectors
    }
    // Last entry
    uint64_t phys_buf          = (uint64_t)virt_to_phys((uint64_t)buf);
    cmdtbl->prdt_entry[i].dba  = phys_buf & 0xFFFFFFFF;
    cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
    cmdtbl->prdt_entry[i].dbc  = (count << 9) - 1; // 512 bytes per sector
    cmdtbl->prdt_entry[i].i    = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c        = 1; // Command
    cmdfis->command  = AHCI_CMD_READ_DMA_EXT;

    cmdfis->lba0   = (uint8_t)startl;
    cmdfis->lba1   = (uint8_t)(startl >> 8);
    cmdfis->lba2   = (uint8_t)(startl >> 16);
    cmdfis->device = 1 << 6; // LBA mode

    cmdfis->lba3 = (uint8_t)(startl >> 24);
    cmdfis->lba4 = (uint8_t)starth;
    cmdfis->lba5 = (uint8_t)(starth >> 8);

    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    // The below loop waits until the port is no longer busy before issuing a new
    // command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        logk("Port is hung\n");
        return false;
    }

    port->ci = 1 << slot; // Issue command

    // Wait for completion
    infinite_loop {
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) // Task file error
        {
            kwarn("Read ahci disk error\n");
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES) {
        kwarn("Read ahci disk error\n");
        return false;
    }

    return true;
}

void ahci_search_ports(HBA_MEM *abar) {
    // Search disk in implemented ports
    uint32_t pi = abar->pi;

    int i = 0;
    while (i < 32) {
        if (pi & 1) {
            int dt = check_type(&abar->ports[i]);
            if (dt == AHCI_DEV_SATA) {
                AHCI_PORT_ARY ary;
                ary.port                 = i;
                ary.type                 = dt;
                ahci_ports[port_total++] = ary;
                logkf("SATA drive found at port %d\n", i);
            } else if (dt == AHCI_DEV_SATAPI) {
                AHCI_PORT_ARY ary;
                ary.port                 = i;
                ary.type                 = dt;
                ahci_ports[port_total++] = ary;
                logkf("SATAPI drive found at port %d\n", i);
            } else if (dt == AHCI_DEV_SEMB) {
                logkf("SEMB drive found at port %d\n", i);
            } else if (dt == AHCI_DEV_PM) {
                logkf("PM drive found at port %d\n", i);
            } else {
            }
        }
        pi >>= 1;
        i++;
    }
}

static void ahci_vdisk_read(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    //                    klog("mapping %d\n",drive_mapping[drive]);
    ahci_read(&(hba_mem->ports[drive_mapping[drive]]), lba, 0, number, (uint16_t *)buffer);
}

static void ahci_vdisk_write(int drive, uint8_t *buffer, uint32_t number, uint32_t lba) {
    ahci_write(&(hba_mem->ports[drive_mapping[drive]]), lba, 0, number, (uint16_t *)buffer);
}

void swap_and_terminate(uint8_t *src, char *dest, size_t len) {
    for (size_t i = 0; i < len; i += 2) {
        if (i + 1 < len) {
            dest[i]     = src[i + 1];
            dest[i + 1] = src[i];
        } else {
            dest[i] = src[i];
        }
    }
    dest[len] = '\0';
}

void ahci_setup() {
    pci_device_t *device = pci_find_class(0x010601);
    if (device == NULL) { return; }

    uint32_t conf  = pci_read_command(device, 0x04);
    conf          |= PCI_RCMD_BUS_MASTER;
    conf          |= PCI_COMMAND_MEMORY;
    conf          |= PCI_COMMAND_IO;
    pci_write_command(device, 0x04, conf);
    hba_mem = (HBA_MEM *)phys_to_virt(device->bars[5].address);

    hba_mem->ghc |= AHCI_GHC_AE;

    ahci_ports_base_addr = (uint64_t)aligned_alloc(PAGE_SIZE, 1048576);

    ahci_search_ports(hba_mem);
    size_t i;
    for (i = 0; i < port_total; i++) {
        ahci_port_rebase(&(hba_mem->ports[ahci_ports[i].port]), ahci_ports[i].port);
    }

    hba_mem->ghc |= AHCI_GHC_IE;

    for (i = 0; i < port_total; i++) {
        SATA_ident_t *buf = malloc(sizeof(SATA_ident_t));
        int a = ahci_identify(&(hba_mem->ports[ahci_ports[i].port]), buf, ahci_ports[i].type);
        if (!a) {
            kwarn("SATA Drive identify error.");
            free(buf);
            continue;
        }

        int type = check_type(&hba_mem->ports[ahci_ports[i].port]);

        if (type == AHCI_DEV_NULL) continue;

        char result[41];
        swap_and_terminate(buf->model, result, 40); // 用于对AHCI设备的model进行字节交换

        kinfo("%s %d Found Drive %dByte - %s",
              type == AHCI_DEV_SATA     ? "SATA"
              : type == AHCI_DEV_SATAPI ? "SATAPI"
              : type == AHCI_DEV_PM     ? "AHCI_PM"
                                        : "AHCI_SEMB",
              i, buf->lba_capacity * 512, result);

        vdisk vd;
        vd.size  = buf->lba_capacity;
        vd.type  = VDISK_BLOCK;
        vd.read  = ahci_vdisk_read;
        vd.write = ahci_vdisk_write;
        vd.flag = type == AHCI_DEV_SATA ? 1 : (type == AHCI_DEV_SATAPI ? 2 : (AHCI_DEV_PM ? 3 : 0));
        vd.sector_size = type == AHCI_DEV_SATA ? 512 : (type == AHCI_DEV_SATAPI ? 2048 : 0);
        char name[10];
        sprintf(name, "sata%d", i);
        strcpy(vd.drive_name, name);

        int id            = regist_vdisk(vd);
        drive_mapping[id] = ahci_ports[i].port;
        free(buf);
    }

    kinfo("AHCI init done - find %d device.", port_total);
}
