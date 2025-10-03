#include "ahci.h"
#include "frame.h"
#include "heap.h"
#include "hhdm.h"
#include "krlibc.h"

void scsi_create_packet12(struct scsi_cdb12 *cdb, uint8_t opcode, uint32_t lba,
                          uint32_t alloc_size) {
    memset(cdb, 0, sizeof(struct scsi_cdb12));
    cdb->opcode = opcode;
    cdb->lba_be = SCSI_FLIP(lba);
    cdb->length = SCSI_FLIP(alloc_size);
}

void scsi_create_packet16(struct scsi_cdb16 *cdb, uint8_t opcode, uint64_t lba,
                          uint32_t alloc_size) {
    memset(cdb, 0, sizeof(struct scsi_cdb16));
    cdb->opcode    = opcode;
    cdb->lba_be_hi = SCSI_FLIP((uint32_t)(lba >> 32));
    cdb->lba_be_lo = SCSI_FLIP((uint32_t)lba);
    cdb->length    = SCSI_FLIP(alloc_size);
}

void scsi_parse_capacity(struct hba_device *device, uint32_t *parameter) {
    if (device->cbd_size == SCSI_CDB16) {
        device->max_lba =
            (uint64_t)SCSI_FLIP(*(parameter + 1)) | ((uint64_t)SCSI_FLIP(*parameter) << 32);
        device->block_size = SCSI_FLIP(*(parameter + 2));
    } else {
        // for READ_CAPACITY(10)
        device->max_lba    = SCSI_FLIP(*(parameter));
        device->block_size = SCSI_FLIP(*(parameter + 1));
    }
}

void scsi_submit(struct hba_device *dev, struct blkio_req *io_req) {
    struct hba_port *port = dev->port;
    struct hba_cmdh *header;
    struct hba_cmdt *table;

    int write = !!(io_req->flags & BLKIO_WRITE);
    int slot  = hba_prepare_cmd(port, &table, &header);

    struct vecbuf *vbuf = NULL;
    vbuf_chunkify(&vbuf, (void *)(uintptr_t)io_req->buf, (size_t)io_req->len, PAGE_SIZE);
    hba_bind_vbuf(header, table, vbuf);
    vbuf_free(vbuf);

    header->options |= HBA_CMDH_ATAPI | (HBA_CMDH_WRITE * write);

    size_t   size  = io_req->len;
    uint32_t count = ICEIL(size, port->device->block_size);

    struct sata_reg_fis *fis = (struct sata_reg_fis *)(&table->command_fis);
    void                *cdb = table->atapi_cmd;
    sata_create_fis(fis, ATA_PACKET, (size << 8), 0);
    fis->feature = 1 | ((!write) << 2);

    if (port->device->cbd_size == SCSI_CDB16) {
        scsi_create_packet16((struct scsi_cdb16 *)cdb,
                             write ? SCSI_WRITE_BLOCKS_16 : SCSI_READ_BLOCKS_16, io_req->lba,
                             count);
    } else {
        scsi_create_packet12((struct scsi_cdb12 *)cdb,
                             write ? SCSI_WRITE_BLOCKS_12 : SCSI_READ_BLOCKS_12,
                             io_req->lba & (uint32_t)-1, count);
    }

    // field: cdb->misc1
    *((uint8_t *)cdb + 1) = 3 << 5; // RPROTECT=011b 禁用保护检查

    // The async way...
    struct hba_cmd_state *cmds = malloc(sizeof(struct hba_cmd_state));
    *cmds                      = (struct hba_cmd_state){.cmd_table = table, .state_ctx = io_req};
    ahci_post(port, cmds, slot);

    free(cmds);
    free_frame((uint64_t)virt_to_phys((uint64_t)table));
}
