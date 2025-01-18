#include "ahci.h"
#include "pci.h"
#include "hhdm.h"
#include "kprint.h"
#include "krlibc.h"
#include "alloc.h"

static uint32_t ahci_ports[32];
static uint32_t port_total = 0;
static uint64_t ahci_ports_base_addr;
HBA_MEM *hba_mem;

static int check_type(HBA_PORT *port) {
    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    if (det != HBA_PORT_DET_PRESENT) return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;

    switch (port->sig) {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            return AHCI_DEV_PM;
        default:
            return AHCI_DEV_SATA;
    }
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
    while (1) {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
}

static int find_cmdslot(HBA_PORT *port) {
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (port->sact | port->ci);
    int cmdslots = (hba_mem->cap & 0x1f00) >> 8;
    for (int i = 0; i < cmdslots; i++) {
        if ((slots & 1) == 0) return i;
        slots >>= 1;
    }
    kwarn("Cannot find free command list entry\n");
    return -1;
}

void ahci_port_rebase(HBA_PORT *port, int portno) {
    stop_cmd(port); // Stop command engine

    // Command list offset: 1K*portno
    // Command list entry size = 32
    // Command list entry maxim count = 32
    // Command list maxim size = 32*32 = 1K per port
    port->clb = ahci_ports_base_addr + (portno << 10);
    port->clbu = 0;
    memset((void *) (port->clb), 0, 1024);

    // FIS offset: 32K+256*portno
    // FIS entry size = 256 bytes per port
    port->fb = ahci_ports_base_addr + (32 << 10) + (portno << 8);
    port->fbu = 0;
    memset((void *) (port->fb), 0, 256);

    // Command table offset: 40K + 8K*portno
    // Command table size = 256*32 = 8K per port
    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *) (port->clb);
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8; // 8 prdt entries per command table
        // 256 bytes per command table, 64+16+48+16*8
        // Command table offset: 40K + 8K*portno + cmdheader_index*256
        cmdheader[i].ctba = ahci_ports_base_addr + (40 << 10) + (portno << 13) + (i << 8);
        cmdheader[i].ctbau = 0;
        memset((void *) cmdheader[i].ctba, 0, 256);
    }

    start_cmd(port); // Start command engine
}

bool ahci_identify(HBA_PORT *port, void *buf) {
    port->is = (uint32_t) -1; // Clear pending interrupt bits
    int spin = 0;       // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *) port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
    cmdheader->w = 0;                                 // Read from device
    cmdheader->prdtl = 1;                                 // PRDT entries count
    cmdheader->c = 1;
    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *) (cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    cmdtbl->prdt_entry[0].dba = (uint64_t)buf;
    cmdtbl->prdt_entry[0].dbau = 0;
    cmdtbl->prdt_entry[0].dbc = 0x200 - 1;
    cmdtbl->prdt_entry[0].i = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *) (&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;    // Command
    cmdfis->command = 0xec; // ATA IDENTIFY

    // The below loop waits until the port is no longer busy before issuing a new
    // command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        kwarn("Port is hung\n");
        return false;
    }

    port->ci = 1 << slot; // Issue command

    // Wait for completion
    while (1) {
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
                ahci_ports[port_total++] = i;
            } else if (dt == AHCI_DEV_SATAPI) {
                ahci_ports[port_total++] = i;
            } else if (dt == AHCI_DEV_SEMB) {
            } else if (dt == AHCI_DEV_PM) {
            } else {
            }
        }

        pi >>= 1;
        i++;
    }
}

void ahci_setup(){
    pci_device_t device = pci_find_class(0x010601);
    if (device == NULL) {
        return;
    }
    int i;

    hba_mem = phys_to_virt((uint64_t)read_bar_n(device,5));

    uint32_t conf = pci_read_command_status(device);
    conf &= 0xffff0000;
    conf |= 0x7; //允许产生中断
    conf |= PCI_RCMD_DISABLE_INTR; // 切换MSI
    conf |= PCI_RCMD_BUS_MASTER; // 启用总线主控制
    conf |= PCI_RCMD_IO_ACCESS; // 启用IO空间访问
    pci_write_command_status(device, conf);

    hba_mem->ghc |= (1 << 31);

    ahci_ports_base_addr = (uint64_t)malloc(1048576);

    ahci_search_ports(hba_mem);
    for (i = 0; i < port_total; i++) {
        ahci_port_rebase(&(hba_mem->ports[ahci_ports[i]]), ahci_ports[i]);
    }

    for (i = 0; i < port_total; i++) {
        SATA_ident_t buf;
        int a = ahci_identify(&(hba_mem->ports[ahci_ports[i]]), &buf);
        if (!a) {
            kwarn("SATA Drive identify error.\n");
            continue;
        }

        int type = check_type(&hba_mem->ports[i]);
        kinfo("SATA %d Found %s Drive %dMB - ", i,
              type == AHCI_DEV_NULL ? "NoDevice" :
              type == AHCI_DEV_SATA ? "AHCI_SATA" :
              type == AHCI_DEV_SATAPI ? "AHCI_SATAPI" :
              type == AHCI_DEV_PM ? "AHCI_PM" :
              "AHCI_SEMB", buf.lba_capacity * 512 / 1024 / 1024);
    }

    kinfo("AHCI init done - find %d device.",port_total);
}
