#ifndef CRASHPOWEROS_AHCI_H
#define CRASHPOWEROS_AHCI_H

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_FR 0x4000
#define HBA_PxCMD_CR 0x8000

#define HBA_PxIS_TFES (1 << 30) /* TFES - Task File Error Status */
#define ATA_CMD_READ_DMA_EX     0x25
#define ATA_CMD_WRITE_DMA_EX     0x35

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08



void ahci_init();

#endif
