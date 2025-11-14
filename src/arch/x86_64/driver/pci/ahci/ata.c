#include "ahci.h"
#include "frame.h"
#include "heap.h"
#include "hhdm.h"
#include "kprint.h"
#include "page.h"

void sata_read_error(struct hba_port *port) {
    printk("SATA read error\n");
    uint32_t tfd                        = port->regs[HBA_RPxTFD];
    port->device->last_result.sense_key = (tfd & 0xf000) >> 12;
    port->device->last_result.error     = (tfd & 0x0f00) >> 8;
    port->device->last_result.status    = tfd & 0x00ff;
}

void sata_submit(struct hba_device *dev, struct blkio_req *io_req) {
    struct hba_port *port = dev->port;
    struct hba_cmdh *header;
    struct hba_cmdt *table;

    int write = !!(io_req->flags & BLKIO_WRITE);
    int slot  = hba_prepare_cmd(port, &table, &header);

    struct vecbuf *vbuf = NULL;
    vbuf_chunkify(&vbuf, (void *)(uintptr_t)io_req->buf, (size_t)io_req->len, PAGE_SIZE);
    hba_bind_vbuf(header, table, vbuf);
    vbuf_free(vbuf);

    header->options |= HBA_CMDH_WRITE * write;

    uint16_t             count = ICEIL(io_req->len, port->device->block_size);
    struct sata_reg_fis *fis   = (struct sata_reg_fis *)(&table->command_fis);

    if ((port->device->flags & HBA_DEV_FEXTLBA)) {
        // 如果该设备支持48位LBA寻址
        sata_create_fis(fis, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, io_req->lba, count);
    } else {
        sata_create_fis(fis, write ? ATA_WRITE_DMA : ATA_READ_DMA, io_req->lba, count);
    }
    fis->dev = (1 << 6);

    // The async way...
    struct hba_cmd_state *cmds = malloc(sizeof(struct hba_cmd_state));
    *cmds                      = (struct hba_cmd_state){.cmd_table = table, .state_ctx = io_req};
    ahci_post(port, cmds, slot);

    free(cmds);
    free_frame((uint64_t)virt_to_phys((uint64_t)table));
}
