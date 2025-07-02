#include "ahci.h"
#include "kprint.h"

int ahci_try_send(struct hba_port *port, int slot) {
    int retries = 0, bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)));

    hba_clear_reg(port->regs[HBA_RPxIS]);

    while (retries < MAX_RETRY) {
        // PxCI寄存器置位，告诉HBA这儿有个数据需要发送到SATA端口
        port->regs[HBA_RPxCI] = bitmask;

        uint64_t counter = wait_until_expire(!(port->regs[HBA_RPxCI] & bitmask), 1000);
        if (counter <= 1) return false;

        port->regs[HBA_RPxCI] &= ~bitmask; // ensure CI bit is cleared
        if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR)) {
            // 有错误
            sata_read_error(port);
            retries++;
        } else {
            break;
        }
    }

    hba_clear_reg(port->regs[HBA_RPxIS]);

    return retries < MAX_RETRY;
}

void ahci_post(struct hba_port *port, struct hba_cmd_state *state, int slot) {
    int bitmask = 1 << slot;

    // 确保端口是空闲的
    uint64_t counter =
        wait_until_expire(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)), 1000);
    if (!counter) {
        printk("AHCI wait timeout\n");
        return;
    }

    hba_clear_reg(port->regs[HBA_RPxIS]);

    port->cmdctx.issued[slot]  = state;
    port->cmdctx.tracked_ci   |= bitmask;
    port->regs[HBA_RPxCI]     |= bitmask;

    while (1) {
        if ((port->regs[HBA_RPxCI] & (1 << slot)) == 0) break;
        if (port->regs[HBA_RPxIS] & HBA_PxINTR_TFE) { goto err; }
    }

    if (port->regs[HBA_RPxIS] & HBA_PxINTR_TFE) {
    err:
        printk("AHCI task file error\n");

        port->regs[HBA_RPxCI] &= ~bitmask;

        hba_clear_reg(port->regs[HBA_RPxIS]);
    }
}
