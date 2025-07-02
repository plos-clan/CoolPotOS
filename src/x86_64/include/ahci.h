#pragma once

#define HBA_RCAP 0
#define HBA_RGHC 1
#define HBA_RIS  2
#define HBA_RPI  3
#define HBA_RVER 4

#define HBA_RPBASE  (0x40)
#define HBA_RPSIZE  (0x80 / sizeof(hba_reg_t))
#define HBA_RPxCLB  0
#define HBA_RPxFB   2
#define HBA_RPxIS   4
#define HBA_RPxIE   5
#define HBA_RPxCMD  6
#define HBA_RPxTFD  8
#define HBA_RPxSIG  9
#define HBA_RPxSSTS 10
#define HBA_RPxSCTL 11
#define HBA_RPxSERR 12
#define HBA_RPxSACT 13
#define HBA_RPxCI   14
#define HBA_RPxSNTF 15
#define HBA_RPxFBS  16

#define HBA_PxCMD_FRE  (1 << 4)
#define HBA_PxCMD_CR   (1 << 15)
#define HBA_PxCMD_FR   (1 << 14)
#define HBA_PxCMD_ST   (1)
#define HBA_PxINTR_DMA (1 << 2)
#define HBA_PxINTR_DHR (1)
#define HBA_PxINTR_DPS (1 << 5)
#define HBA_PxINTR_TFE (1 << 30)
#define HBA_PxINTR_HBF (1 << 29)
#define HBA_PxINTR_HBD (1 << 28)
#define HBA_PxINTR_IF  (1 << 27)
#define HBA_PxINTR_NIF (1 << 26)
#define HBA_PxINTR_OF  (1 << 24)
#define HBA_PxTFD_ERR  (1)
#define HBA_PxTFD_BSY  (1 << 7)
#define HBA_PxTFD_DRQ  (1 << 3)

#define HBA_FATAL (HBA_PxINTR_TFE | HBA_PxINTR_HBF | HBA_PxINTR_HBD | HBA_PxINTR_IF)

#define HBA_NONFATAL (HBA_PxINTR_NIF | HBA_PxINTR_OF)

#define HBA_RGHC_ACHI_ENABLE ((uint32_t)1 << 31)
#define HBA_RGHC_INTR_ENABLE ((uint32_t)1 << 1)
#define HBA_RGHC_RESET       1

#define HBA_RPxSSTS_PWR(x)      (((x) >> 8) & 0xf)
#define HBA_RPxSSTS_IF(x)       (((x) >> 4) & 0xf)
#define HBA_RPxSSTS_PHYSTATE(x) ((x) & 0xf)

#define hba_clear_reg(reg) (reg) = (hba_reg_t)(-1)

#define HBA_DEV_SIG_ATAPI 0xeb140101
#define HBA_DEV_SIG_ATA   0x00000101

#define __HBA_PACKED__ __attribute__((packed))

#define HBA_CMDH_FIS_LEN(fis_bytes) (((fis_bytes) / 4) & 0x1f)
#define HBA_CMDH_ATAPI              (1 << 5)
#define HBA_CMDH_WRITE              (1 << 6)
#define HBA_CMDH_PREFETCH           (1 << 7)
#define HBA_CMDH_R                  (1 << 8)
#define HBA_CMDH_CLR_BUSY           (1 << 10)
#define HBA_CMDH_PRDT_LEN(entries)  (((entries) & 0xffff) << 16)

#define SCSI_CDB16 16
#define SCSI_CDB12 12

#define HBA_MAX_PRDTE 4

#define SCSI_READ_CAPACITY_16 0x9e
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_BLOCKS_16   0x88
#define SCSI_READ_BLOCKS_12   0xa8
#define SCSI_WRITE_BLOCKS_16  0x8a
#define SCSI_WRITE_BLOCKS_12  0xaa

#define SATA_REG_FIS_D2H                0x34
#define SATA_REG_FIS_H2D                0x27
#define SATA_REG_FIS_COMMAND            0x80
#define SATA_LBA_COMPONENT(lba, offset) ((uint8_t)(((lba) >> (offset)) & 0xff))

#define ATA_IDENTIFY_DEVICE        0xec
#define ATA_IDENTIFY_PAKCET_DEVICE 0xa1
#define ATA_PACKET                 0xa0
#define ATA_READ_DMA_EXT           0x25
#define ATA_READ_DMA               0xc8
#define ATA_WRITE_DMA_EXT          0x35
#define ATA_WRITE_DMA              0xca

// ATA 状态寄存器 (STS) 位定义
#define ATA_STS_ERR (1 << 0) // Error
#define ATA_STS_DRQ (1 << 3) // Data Request
#define ATA_STS_BSY (1 << 7) // Busy

// ATA 错误寄存器 (ERR) 位定义
#define ATA_ERR_AMNF  (1 << 0) // Address Mark Not Found
#define ATA_ERR_TKZNF (1 << 1) // Track Zero Not Found
#define ATA_ERR_ABRT  (1 << 2) // Command Aborted
#define ATA_ERR_IDNF  (1 << 4) // ID Not Found (LBA out of range)
#define ATA_ERR_UNC   (1 << 6) // Uncorrectable Data Error

// AHCI 命令寄存器 (PxCMD) 位定义
#define AHCI_CMD_FR (1 << 14) // FIS Receive Running
#define AHCI_CMD_CR (1 << 15) // Command Running

#define IDDEV_OFFMAXLBA       60
#define IDDEV_OFFMAXLBA_EXT   230
#define IDDEV_OFFLSECSIZE     117
#define IDDEV_OFFWWN          108
#define IDDEV_OFFSERIALNUM    10
#define IDDEV_OFFMODELNUM     27
#define IDDEV_OFFADDSUPPORT   69
#define IDDEV_OFFA48SUPPORT   83
#define IDDEV_OFFALIGN        209
#define IDDEV_OFFLPP          106
#define IDDEV_OFFCAPABILITIES 49

#define HBA_FIS_SIZE 256
#define HBA_CLB_SIZE 1024

#define MAX_RETRY        2
#define AHCI_MAX_DEVICES 20

#define HBA_PRDTE_BYTE_CNT(cnt) ((cnt & 0x3FFFFF) | 0x1)

#define BLKIO_WRITE (1UL << 0)

#define ICEIL(x, y) ((x) / (y) + ((x) % (y) != 0))

#define SCSI_FLIP(val)                                                                             \
    ((((val) & 0x000000ff) << 24) | (((val) & 0x0000ff00) << 8) | (((val) & 0x00ff0000) >> 8) |    \
     (((val) & 0xff000000) >> 24))

#define wait_until(cond)                                                                           \
    ({                                                                                             \
        while (!(cond))                                                                            \
            ;                                                                                      \
    })

#define wait_until_expire(cond, max)                                                               \
    ({                                                                                             \
        uint64_t __wcounter__ = (max);                                                             \
        while (!(cond) && __wcounter__-- > 1)                                                      \
            ;                                                                                      \
        __wcounter__;                                                                              \
    })

#include "ctype.h"

typedef uint32_t hba_reg_t;

struct blkio_req {
    uint64_t buf;
    uint64_t lba;
    uint64_t len;
    uint64_t flags;
};

struct hba_cmdh {
    uint16_t          options;
    uint16_t          prdt_len;
    volatile uint32_t transferred_size;
    uint32_t          cmd_table_base;
    uint32_t          cmd_table_base_upper;
    uint32_t          reserved[4];
} __HBA_PACKED__;

struct hba_prdte {
    uint32_t data_base;
    uint32_t data_base_upper;
    uint32_t reserved;
    uint32_t byte_count : 22;
    uint32_t rsv1       : 9;
    uint32_t i          : 1;
} __HBA_PACKED__;

struct hba_cmdt {
    uint8_t          command_fis[64];
    uint8_t          atapi_cmd[16];
    uint8_t          reserved[0x30];
    struct hba_prdte entries[HBA_MAX_PRDTE];
} __HBA_PACKED__;

#define HBA_DEV_FEXTLBA 1
#define HBA_DEV_FATAPI  (1 << 1)

struct hba_port;
struct ahci_hba;

struct hba_device {
    char     serial_num[20];
    char     model[40];
    uint32_t flags;
    uint64_t max_lba;
    uint32_t block_size;
    uint64_t wwn;
    uint8_t  cbd_size;
    struct {
        uint8_t sense_key;
        uint8_t error;
        uint8_t status;
        uint8_t reserve;
    } last_result;
    uint32_t         alignment_offset;
    uint32_t         block_per_sec;
    uint32_t         capabilities;
    struct hba_port *port;
    struct ahci_hba *hba;

    struct {
        void (*submit)(struct hba_device *dev, struct blkio_req *io_req);
    } ops;
};

struct hba_cmd_state {
    struct hba_cmdt *cmd_table;
    void            *state_ctx;
};

struct hba_cmd_context {
    struct hba_cmd_state *issued[32];
    uint32_t              tracked_ci;
};

struct hba_port {
    volatile hba_reg_t    *regs;
    uint32_t               ssts;
    struct hba_cmdh       *cmdlst;
    struct hba_cmd_context cmdctx;
    void                  *fis;
    struct hba_device     *device;
    struct ahci_hba       *hba;
};

struct ahci_hba {
    volatile hba_reg_t *base;
    uint32_t            ports_num;
    uint32_t            ports_bmp;
    uint32_t            cmd_slots;
    uint32_t            version;
    struct hba_port    *ports[32];
};

struct scsi_cdb12 {
    uint8_t  opcode;
    uint8_t  misc1;
    uint32_t lba_be;
    uint32_t length;
    uint8_t  misc2;
    uint8_t  ctrl;
} __attribute__((packed));

struct scsi_cdb16 {
    uint8_t  opcode;
    uint8_t  misc1;
    uint32_t lba_be_hi;
    uint32_t lba_be_lo;
    uint32_t length;
    uint8_t  misc2;
    uint8_t  ctrl;
} __attribute__((packed));

struct sata_fis_head {
    uint8_t type;
    uint8_t options;
    uint8_t status_cmd;
    uint8_t feat_err;
} __HBA_PACKED__;

struct sata_reg_fis {
    struct sata_fis_head head;

    uint8_t lba0, lba8, lba16;
    uint8_t dev;
    uint8_t lba24, lba32, lba40;
    uint8_t feature;

    uint16_t count;

    uint8_t reserved[6];
} __HBA_PACKED__;

struct sata_data_fis {
    struct sata_fis_head head;

    uint8_t data[0];
} __HBA_PACKED__;

void scsi_submit(struct hba_device *dev, struct blkio_req *io_req);
void sata_submit(struct hba_device *dev, struct blkio_req *io_req);
void sata_read_error(struct hba_port *port);
void ahci_post(struct hba_port *port, struct hba_cmd_state *state, int slot);
int  ahci_try_send(struct hba_port *port, int slot);
void achi_register_ops(struct hba_port *port);
void ahci_parsestr(char *str, uint16_t *reg_start, int size_word);
void ahci_parse_dev_info(struct hba_device *dev_info, uint16_t *data);
int  __get_free_slot(struct hba_port *port);
int  hba_prepare_cmd(struct hba_port *port, struct hba_cmdt **cmdt, struct hba_cmdh **cmdh);
void __hba_reset_port(hba_reg_t *port_reg);
int  hba_bind_sbuf(struct hba_cmdh *cmdh, struct hba_cmdt *cmdt, void *buf, uint32_t len);
void sata_create_fis(struct sata_reg_fis *cmd_fis, uint8_t command, uint64_t lba,
                     uint16_t sector_count);
int  ahci_init_device(struct hba_port *port);
void scsi_create_packet12(struct scsi_cdb12 *cdb, uint8_t opcode, uint32_t lba,
                          uint32_t alloc_size);
void scsi_create_packet16(struct scsi_cdb16 *cdb, uint8_t opcode, uint64_t lba,
                          uint32_t alloc_size);
void scsi_parse_capacity(struct hba_device *device, uint32_t *parameter);
void ahci_setup();
