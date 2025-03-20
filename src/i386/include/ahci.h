#pragma once

#define SATA_SIG_ATA   0x00000101 // SATA drive
#define SATA_SIG_ATAPI 0xEB140101 // SATAPI drive
#define SATA_SIG_SEMB  0xC33C0101 // Enclosure management bridge
#define SATA_SIG_PM    0x96690101 // Port multiplier
#define HBA_PxCMD_ST   0x0001
#define HBA_PxCMD_FRE  0x0010
#define HBA_PxCMD_FR   0x4000
#define HBA_PxCMD_CR   0x8000
#define HBA_PxIS_TFES  (1 << 30) /* TFES - Task File Error Status */

#define AHCI_DEV_NULL   0
#define AHCI_DEV_SATA   1
#define AHCI_DEV_SEMB   2
#define AHCI_DEV_PM     3
#define AHCI_DEV_SATAPI 4

#define ATA_DEV_BUSY           0x80
#define ATA_DEV_DRQ            0x08
#define AHCI_CMD_READ_DMA_EXT  0x25
#define AHCI_CMD_WRITE_DMA_EXT 0x35

#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

#include "ctypes.h"

typedef enum {
    FIS_TYPE_REG_H2D   = 0x27, // Register FIS - host to device
    FIS_TYPE_REG_D2H   = 0x34, // Register FIS - device to host
    FIS_TYPE_DMA_ACT   = 0x39, // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA      = 0x46, // Data FIS - bidirectional
    FIS_TYPE_BIST      = 0x58, // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS  = 0xA1, // Set device bits FIS - device to host
} FIS_TYPE;

typedef volatile struct tagHBA_PORT {
    uint32_t clb;       // 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;      // 0x04, command list base address upper 32 bits
    uint32_t fb;        // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;       // 0x0C, FIS base address upper 32 bits
    uint32_t is;        // 0x10, interrupt status
    uint32_t ie;        // 0x14, interrupt enable
    uint32_t cmd;       // 0x18, command and status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, task file data
    uint32_t sig;       // 0x24, signature
    uint32_t ssts;      // 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;      // 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;      // 0x30, SATA error (SCR1:SError)
    uint32_t sact;      // 0x34, SATA active (SCR3:SActive)
    uint32_t ci;        // 0x38, command issue
    uint32_t sntf;      // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;       // 0x40, FIS-based switch control
    uint32_t rsv1[11];  // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4]; // 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;     // 0x00, Host capability
    uint32_t ghc;     // 0x04, Global host control
    uint32_t is;      // 0x08, Interrupt status
    uint32_t pi;      // 0x0C, Port implemented
    uint32_t vs;      // 0x10, Version
    uint32_t ccc_ctl; // 0x14, Command completion coalescing control
    uint32_t ccc_pts; // 0x18, Command completion coalescing ports
    uint32_t em_loc;  // 0x1C, Enclosure management location
    uint32_t em_ctl;  // 0x20, Enclosure management control
    uint32_t cap2;    // 0x24, Host capabilities extended
    uint32_t bohc;    // 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    uint8_t rsv[0xA0 - 0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t vendor[0x100 - 0xA0];

    // 0x100 - 0x10FF, Port control registers
    HBA_PORT ports[1]; // 1 ~ 32
} HBA_MEM;

typedef struct tagHBA_CMD_HEADER {
    // DW0
    uint8_t cfl : 5; // Command FIS length in DWORDS, 2 ~ 16
    uint8_t a   : 1; // ATAPI
    uint8_t w   : 1; // Write, 1: H2D, 0: D2H
    uint8_t p   : 1; // Prefetchable

    uint8_t r    : 1; // Reset
    uint8_t b    : 1; // BIST
    uint8_t c    : 1; // Clear busy upon R_OK
    uint8_t rsv0 : 1; // Reserved
    uint8_t pmp  : 4; // Port multiplier port

    uint16_t prdtl; // Physical region descriptor table length in entries

    // DW1
    volatile uint32_t prdbc; // Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t ctba;  // Command table descriptor base address
    uint32_t ctbau; // Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t rsv1[4]; // Reserved
} HBA_CMD_HEADER;

typedef struct tagFIS_REG_H2D {
    // DWORD 0
    uint8_t fis_type; // FIS_TYPE_REG_H2D

    uint8_t pmport : 4; // Port multiplier
    uint8_t rsv0   : 3; // Reserved
    uint8_t c      : 1; // 1: Command, 0: Control

    uint8_t command;  // Command register
    uint8_t featurel; // Feature register, 7:0

    // DWORD 1
    uint8_t lba0;   // LBA low register, 7:0
    uint8_t lba1;   // LBA mid register, 15:8
    uint8_t lba2;   // LBA high register, 23:16
    uint8_t device; // Device register

    // DWORD 2
    uint8_t lba3;     // LBA register, 31:24
    uint8_t lba4;     // LBA register, 39:32
    uint8_t lba5;     // LBA register, 47:40
    uint8_t featureh; // Feature register, 15:8

    // DWORD 3
    uint8_t countl;  // Count register, 7:0
    uint8_t counth;  // Count register, 15:8
    uint8_t icc;     // Isochronous command completion
    uint8_t control; // Control register

    // DWORD 4
    uint8_t rsv1[4]; // Reserved
} FIS_REG_H2D;

typedef struct tagHBA_PRDT_ENTRY {
    uint32_t dba;  // Data base address
    uint32_t dbau; // Data base address upper 32 bits
    uint32_t rsv0; // Reserved

    // DW3
    uint32_t dbc  : 22; // Byte count, 4M max
    uint32_t rsv1 : 9;  // Reserved
    uint32_t i    : 1;  // Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL {
    // 0x00
    uint8_t cfis[64]; // Command FIS

    // 0x40
    uint8_t acmd[16]; // ATAPI command, 12 or 16 bytes

    // 0x50
    uint8_t rsv[48]; // Reserved

    // 0x80
    HBA_PRDT_ENTRY prdt_entry[1]; // Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;

typedef struct SATA_Ident {
    uint16_t config;        /* lots of obsolete bit flags */
    uint16_t cyls;          /* obsolete */
    uint16_t reserved2;     /* special config */
    uint16_t heads;         /* "physical" heads */
    uint16_t track_bytes;   /* unformatted bytes per track */
    uint16_t sector_bytes;  /* unformatted bytes per sector */
    uint16_t sectors;       /* "physical" sectors per track */
    uint16_t vendor0;       /* vendor unique */
    uint16_t vendor1;       /* vendor unique */
    uint16_t vendor2;       /* vendor unique */
    uint8_t  serial_no[20]; /* 0 = not_specified */
    uint16_t buf_type;
    uint16_t buf_size;             /* 512 byte increments; 0 = not_specified */
    uint16_t ecc_bytes;            /* for r/w long cmds; 0 = not_specified */
    uint8_t  fw_rev[8];            /* 0 = not_specified */
    uint8_t  model[40];            /* 0 = not_specified */
    uint16_t multi_count;          /* Multiple Count */
    uint16_t dword_io;             /* 0=not_implemented; 1=implemented */
    uint16_t capability1;          /* vendor unique */
    uint16_t capability2;          /* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup word: 50 */
    uint8_t  vendor5;              /* vendor unique */
    uint8_t  tPIO;                 /* 0=slow, 1=medium, 2=fast */
    uint8_t  vendor6;              /* vendor unique */
    uint8_t  tDMA;                 /* 0=slow, 1=medium, 2=fast */
    uint16_t field_valid;          /* bits 0:cur_ok 1:eide_ok */
    uint16_t cur_cyls;             /* logical cylinders */
    uint16_t cur_heads;            /* logical heads word 55*/
    uint16_t cur_sectors;          /* logical sectors per track */
    uint16_t cur_capacity0;        /* logical total sectors on drive */
    uint16_t cur_capacity1;        /*  (2 words, misaligned int)     */
    uint8_t  multsect;             /* current multiple sector count */
    uint8_t  multsect_valid;       /* when (bit0==1) multsect is ok */
    uint32_t lba_capacity;         /* total number of sectors */
    uint16_t dma_1word;            /* single-word dma info */
    uint16_t dma_mword;            /* multiple-word dma info */
    uint16_t eide_pio_modes;       /* bits 0:mode3 1:mode4 */
    uint16_t eide_dma_min;         /* min mword dma cycle time (ns) */
    uint16_t eide_dma_time;        /* recommended mword dma cycle time (ns) */
    uint16_t eide_pio;             /* min cycle time (ns), no IORDY  */
    uint16_t eide_pio_iordy;       /* min cycle time (ns), with IORDY */
    uint16_t words69_70[2];        /* reserved words 69-70 */
    uint16_t words71_74[4];        /* reserved words 71-74 */
    uint16_t queue_depth;          /*  */
    uint16_t sata_capability;      /*  SATA Capabilities word 76*/
    uint16_t sata_additional;      /*  Additional Capabilities */
    uint16_t sata_supported;       /* SATA Features supported  */
    uint16_t features_enabled;     /* SATA features enabled */
    uint16_t major_rev_num;        /*  Major rev number word 80 */
    uint16_t minor_rev_num;        /*  */
    uint16_t command_set_1;        /* bits 0:Smart 1:Security 2:Removable 3:PM */
    uint16_t command_set_2;        /* bits 14:Smart Enabled 13:0 zero */
    uint16_t cfsse;                /* command set-feature supported extensions */
    uint16_t cfs_enable_1;         /* command set-feature enabled */
    uint16_t cfs_enable_2;         /* command set-feature enabled */
    uint16_t csf_default;          /* command set-feature default */
    uint16_t dma_ultra;            /*  */
    uint16_t word89;               /* reserved (word 89) */
    uint16_t word90;               /* reserved (word 90) */
    uint16_t CurAPMvalues;         /* current APM values */
    uint16_t word92;               /* reserved (word 92) */
    uint16_t comreset;             /* should be cleared to 0 */
    uint16_t accoustic;            /*  accoustic management */
    uint16_t min_req_sz;           /* Stream minimum required size */
    uint16_t transfer_time_dma;    /* Streaming Transfer Time-DMA */
    uint16_t access_latency;       /* Streaming access latency-DMA & PIO WORD 97*/
    uint32_t perf_granularity;     /* Streaming performance granularity */
    uint32_t total_usr_sectors[2]; /* Total number of user addressable sectors */
    uint16_t transfer_time_pio;    /* Streaming Transfer time PIO */
    uint16_t reserved105;          /* Word 105 */
    uint16_t sector_sz;            /* Puysical Sector size / Logical sector size */
    uint16_t inter_seek_delay;     /* In microseconds */
    uint16_t words108_116[9];      /*  */
    uint32_t words_per_sector;     /* words per logical sectors */
    uint16_t supported_settings;   /* continued from words 82-84 */
    uint16_t command_set_3;        /* continued from words 85-87 */
    uint16_t words121_126[6];      /* reserved words 121-126 */
    uint16_t word127;              /* reserved (word 127) */
    uint16_t security_status;      /* device lock function
                                      * 15:9   reserved
                                      * 8   security level 1:max 0:high
                                      * 7:6   reserved
                                      * 5   enhanced erase
                                      * 4   expire
                                      * 3   frozen
                                      * 2   locked
                                      * 1   en/disabled
                                      * 0   capability
                                      */
    uint16_t csfo;                 /* current set features options
                                      * 15:4   reserved
                                      * 3   auto reassign
                                      * 2   reverting
                                      * 1   read-look-ahead
                                      * 0   write cache
                                      */
    uint16_t words130_155[26];     /* reserved vendor words 130-155 */
    uint16_t word156;
    uint16_t words157_159[3];     /* reserved vendor words 157-159 */
    uint16_t cfa;                 /* CFA Power mode 1 */
    uint16_t words161_175[15];    /* Reserved */
    uint8_t  media_serial[60];    /* words 176-205 Current Media serial number */
    uint16_t sct_cmd_transport;   /* SCT Command Transport */
    uint16_t words207_208[2];     /* reserved */
    uint16_t block_align;         /* Alignement of logical blocks in larger physical blocks */
    uint32_t WRV_sec_count;       /* Write-Read-Verify sector count mode 3 only */
    uint32_t verf_sec_count;      /* Verify Sector count mode 2 only */
    uint16_t nv_cache_capability; /* NV Cache capabilities */
    uint16_t nv_cache_sz;         /* NV Cache size in logical blocks */
    uint16_t nv_cache_sz2;        /* NV Cache size in logical blocks */
    uint16_t rotation_rate;       /* Nominal media rotation rate */
    uint16_t reserved218;         /*  */
    uint16_t nv_cache_options;    /* NV Cache options */
    uint16_t words220_221[2];     /* reserved */
    uint16_t transport_major_rev; /*  */
    uint16_t transport_minor_rev; /*  */
    uint16_t words224_233[10];    /* Reserved */
    uint16_t min_dwnload_blocks;  /* Minimum number of 512byte units per
                             DOWNLOAD MICROCODE  command for mode 03h */
    uint16_t max_dwnload_blocks;  /* Maximum number of 512byte units per
                             DOWNLOAD MICROCODE  command for mode 03h */
    uint16_t words236_254[19];    /* Reserved */
    uint16_t integrity;           /* Cheksum, Signature */
} SATA_ident_t;

void ahci_init();
void ahci_search_ports(HBA_MEM *abar);
void ahci_port_rebase(HBA_PORT *port, int portno);
bool ahci_identify(HBA_PORT *port, void *buf);
bool ahci_read(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf);
bool ahci_write(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf);