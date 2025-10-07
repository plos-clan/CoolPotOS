#include "xhci_hcd.h"
#include "errno.h"
#include "int_subsystem.h"
#include "mem_subsystem.h"
#include "proc_subsystem.h"

// 前向声明
static int xhci_hcd_init(usb_hcd_t *hcd);
static int xhci_hcd_shutdown(usb_hcd_t *hcd);
static int xhci_reset_port(usb_hcd_t *hcd, uint8_t port);
static int xhci_enable_slot(usb_hcd_t *hcd, usb_device_t *device);
static int xhci_disable_slot(usb_hcd_t *hcd, usb_device_t *device);
static int xhci_address_device(usb_hcd_t *hcd, usb_device_t *device, uint8_t address);
static int xhci_configure_endpoint(usb_hcd_t *hcd, usb_endpoint_t *endpoint);
static int xhci_control_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer,
                                 usb_device_request_t *setup);
static int xhci_bulk_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer);
static int xhci_interrupt_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer);

static usb_hcd_ops_t xhci_ops = {
    .init               = xhci_hcd_init,
    .shutdown           = xhci_hcd_shutdown,
    .reset_port         = xhci_reset_port,
    .enable_slot        = xhci_enable_slot,
    .disable_slot       = xhci_disable_slot,
    .address_device     = xhci_address_device,
    .configure_endpoint = xhci_configure_endpoint,
    .control_transfer   = xhci_control_transfer,
    .bulk_transfer      = xhci_bulk_transfer,
    .interrupt_transfer = xhci_interrupt_transfer,
};

spin_t transfer_lock = SPIN_INIT;

// 分配DMA内存（对齐到64字节）
static void *xhci_alloc_dma(size_t size, uint64_t *phys_addr) {
    void *ptr = malloc((size + 63) & ~63);
    if (ptr) {
        memset(ptr, 0, size);
        if (phys_addr) { *phys_addr = page_virt_to_phys((uint64_t)ptr); }
    }
    return ptr;
}

// 分配传输环
xhci_ring_t *xhci_alloc_ring(uint32_t num_trbs) {
    xhci_ring_t *ring = (xhci_ring_t *)malloc(sizeof(xhci_ring_t));
    if (!ring) { return NULL; }

    memset(ring, 0, sizeof(xhci_ring_t));
    ring->size = num_trbs;
    ring->trbs = (xhci_trb_t *)xhci_alloc_dma(sizeof(xhci_trb_t) * num_trbs, &ring->phys_addr);
    if (!ring->trbs) {
        free(ring);
        return NULL;
    }

    ring->cycle_state   = 1;
    ring->enqueue_index = 0;
    ring->dequeue_index = 0;

    return ring;
}

// 释放传输环
void xhci_free_ring(xhci_ring_t *ring) {
    if (!ring) return;

    if (ring->trbs) { free(ring->trbs); }
    free(ring);
}

// 向环中添加TRB
xhci_trb_t *xhci_queue_trb(xhci_ring_t *ring, uint64_t param, uint32_t status, uint32_t control) {
    if (!ring || !ring->trbs) {
        printk("XHCI: Invalid ring in queue_trb\n");
        return NULL;
    }

    spin_lock(transfer_lock);

    uint32_t idx = ring->enqueue_index;

    // 检查是否需要 Link TRB
    if (idx >= ring->size - 1) {
        // 设置 Link TRB
        xhci_trb_t *link = &ring->trbs[ring->size - 1];
        link->parameter  = ring->phys_addr;
        link->status     = 0;
        link->control    = (TRB_TYPE_LINK << 10);

        link->control |= TRB_LINK_TOGGLE_CYCLE;

        // 设置 cycle bit
        if (ring->cycle_state) { link->control |= TRB_FLAG_CYCLE; }

        // 翻转 cycle state
        ring->cycle_state   = !ring->cycle_state;
        ring->enqueue_index = 0;
        idx                 = 0;
    }

    xhci_trb_t *trb = &ring->trbs[idx];

    // 填充 TRB（先不设置 cycle bit）
    trb->parameter = param;
    trb->status    = status;
    trb->control   = control & ~TRB_FLAG_CYCLE;

    // 设置 cycle bit（激活 TRB）
    if (ring->cycle_state) { trb->control |= TRB_FLAG_CYCLE; }

    // 更新 enqueue 指针
    ring->enqueue_index = (idx + 1) % ring->size;

    spin_unlock(transfer_lock);

    return trb;
}

// 敲门铃寄存器
void xhci_ring_doorbell(xhci_hcd_t *xhci, uint8_t slot_id, uint8_t dci) {
    xhci_writel(&xhci->doorbell_regs[slot_id], dci);
}

// 重置XHCI控制器
int xhci_reset(xhci_hcd_t *xhci) {
    uint32_t cmd, status;
    int      timeout = 1000;

    // 重置控制器
    cmd  = xhci_readl(&xhci->op_regs->usbcmd);
    cmd |= XHCI_CMD_RESET;
    xhci_writel(&xhci->op_regs->usbcmd, cmd);

    // 等待重置完成
    timeout = 1000;
    while (timeout--) {
        cmd    = xhci_readl(&xhci->op_regs->usbcmd);
        status = xhci_readl(&xhci->op_regs->usbsts);
        if (!(cmd & XHCI_CMD_RESET) && !(status & XHCI_STS_CNR)) { break; }
    }

    if (timeout <= 0) {
        printk("XHCI: Reset timeout\n");
        return -1;
    }

    printk("XHCI: Reset complete\n");
    return 0;
}

// 启动XHCI控制器
int xhci_start(xhci_hcd_t *xhci) {
    uint32_t cmd;

    printk("XHCI: Starting controller...\n");

    cmd  = xhci_readl(&xhci->op_regs->usbcmd);
    cmd |= XHCI_CMD_RUN;
    xhci_writel(&xhci->op_regs->usbcmd, cmd);

    // 等待控制器运行
    int timeout = 1000;
    while (timeout--) {
        uint32_t status = xhci_readl(&xhci->op_regs->usbsts);
        if (!(status & XHCI_STS_HCH)) {
            printk("XHCI: Controller running\n");
            return 0;
        }
    }

    printk("XHCI: Failed to start controller\n");
    return -1;
}

// 停止XHCI控制器
int xhci_stop(xhci_hcd_t *xhci) {
    uint32_t cmd;

    printk("XHCI: Stopping controller...\n");

    cmd  = xhci_readl(&xhci->op_regs->usbcmd);
    cmd &= ~XHCI_CMD_RUN;
    xhci_writel(&xhci->op_regs->usbcmd, cmd);

    // 等待控制器停止
    int timeout = 1000;
    while (timeout--) {
        uint32_t status = xhci_readl(&xhci->op_regs->usbsts);
        if (status & XHCI_STS_HCH) {
            printk("XHCI: Controller stopped\n");
            return 0;
        }
    }

    printk("XHCI: Failed to stop controller\n");
    return -1;
}

// 扩展能力 ID
#define XHCI_EXT_CAPS_PROTOCOL 2

// 解析 Supported Protocol Capability
static int xhci_parse_protocol_caps(xhci_hcd_t *xhci) {
    uint32_t hccparams1 = xhci_readl(&xhci->cap_regs->hccparams1);
    uint32_t ext_offset = (hccparams1 >> 16) & 0xFFFF;

    if (ext_offset == 0) {
        printk("XHCI: No extended capabilities\n");
        return 0;
    }

    printk("XHCI: Parsing extended capabilities (offset: 0x%x)\n", ext_offset * 4);

    // 扩展能力在能力寄存器之后
    uint32_t *ext_cap = (uint32_t *)((uint8_t *)xhci->cap_regs + (ext_offset * 4));

    while (ext_offset) {
        uint32_t cap_header  = xhci_readl(ext_cap);
        uint32_t cap_id      = cap_header & 0xFF;
        uint32_t next_offset = (cap_header >> 8) & 0xFF;

        printk("  Capability ID: %u, Next: %u\n", cap_id, next_offset);

        if (cap_id == XHCI_EXT_CAPS_PROTOCOL) {
            // Supported Protocol Capability
            uint32_t cap_word0 = xhci_readl(ext_cap);
            uint32_t cap_word1 = xhci_readl(ext_cap + 1);
            uint32_t cap_word2 = xhci_readl(ext_cap + 2);

            // 解析协议版本
            uint8_t minor = (cap_word2 >> 16) & 0xFF;
            uint8_t major = (cap_word2 >> 24) & 0xFF;

            // 解析端口范围
            uint8_t port_offset = cap_word2 & 0xFF;
            uint8_t port_count  = (cap_word2 >> 8) & 0xFF;

            printk("  Protocol: USB %u.%u\n", major, minor);
            printk("  Port Offset: %u\n", port_offset);
            printk("  Port Count: %u\n", port_count);

            // 标记端口协议
            for (int i = 0; i < port_count && (port_offset + i - 1) < xhci->max_ports; i++) {
                uint8_t port_idx = port_offset + i - 1; // 转换为 0-based

                xhci->port_info[port_idx].port_num    = port_idx;
                xhci->port_info[port_idx].protocol    = major;
                xhci->port_info[port_idx].port_offset = i;
                xhci->port_info[port_idx].port_count  = port_count;

                printk("    Port %u: USB%u\n", port_idx, major);
            }
        }

        if (next_offset == 0) { break; }

        ext_cap    += next_offset;
        ext_offset  = next_offset;
    }

    return 0;
}

void xhci_handle_port_status(xhci_hcd_t *xhci, uint8_t port_id);

// 初始化XHCI HCD
static int xhci_hcd_init(usb_hcd_t *hcd) {
    xhci_hcd_t *xhci = (xhci_hcd_t *)hcd->private_data;

    xhci->hcd = hcd;

    printk("XHCI: Initializing controller\n");

    xhci->tracker_mutex = SPIN_INIT;

    // 读取能力参数
    uint32_t hcsparams1 = xhci_readl(&xhci->cap_regs->hcsparams1);
    uint32_t hcsparams2 = xhci_readl(&xhci->cap_regs->hcsparams2);
    uint32_t hccparams1 = xhci_readl(&xhci->cap_regs->hccparams1);

    xhci->max_slots = (hcsparams1 >> 0) & 0xFF;
    xhci->max_intrs = (hcsparams1 >> 8) & 0x7FF;
    xhci->max_ports = (hcsparams1 >> 24) & 0xFF;

    printk("XHCI: Max slots=%d, ports=%d, interrupters=%d\n", xhci->max_slots, xhci->max_ports,
           xhci->max_intrs);

    // 计算scratchpad数量
    uint32_t max_scratch_hi = (hcsparams2 >> 21) & 0x1F;
    uint32_t max_scratch_lo = (hcsparams2 >> 27) & 0x1F;
    xhci->num_scratchpads   = (max_scratch_hi << 5) | max_scratch_lo;

    // 重置控制器
    if (xhci_reset(xhci) != 0) { return -1; }

    // 配置max slots
    uint32_t config  = xhci_readl(&xhci->op_regs->config);
    config          &= ~0xFF;
    config          |= xhci->max_slots;
    xhci_writel(&xhci->op_regs->config, config);

    // 分配设备上下文基地址数组 (DCBAA)
    xhci->dcbaa =
        (uint64_t *)xhci_alloc_dma((xhci->max_slots + 1) * sizeof(uint64_t), &xhci->dcbaa_phys);
    if (!xhci->dcbaa) {
        printk("XHCI: Failed to allocate DCBAA\n");
        return -1;
    }

    // 分配scratchpad buffers
    if (xhci->num_scratchpads > 0) {
        xhci->scratchpad_array =
            (uint64_t *)xhci_alloc_dma(xhci->num_scratchpads * sizeof(uint64_t), NULL);
        xhci->scratchpad_buffers = (void **)malloc(xhci->num_scratchpads * sizeof(void *));

        for (uint32_t i = 0; i < xhci->num_scratchpads; i++) {
            uint64_t phys;
            xhci->scratchpad_buffers[i] = xhci_alloc_dma(4096, &phys);
            xhci->scratchpad_array[i]   = phys;
        }

        xhci->dcbaa[0] = page_virt_to_phys((uint64_t)xhci->scratchpad_array);
    }

    // 设置DCBAA指针
    xhci_writeq(&xhci->op_regs->dcbaap, xhci->dcbaa_phys);

    // 分配命令环
    xhci->cmd_ring = xhci_alloc_ring(256);
    if (!xhci->cmd_ring) {
        printk("XHCI: Failed to allocate command ring\n");
        return -1;
    }

    // 设置命令环指针
    xhci_writeq(&xhci->op_regs->crcr, xhci->cmd_ring->phys_addr | 1);

    // 分配事件环
    xhci->event_ring = xhci_alloc_ring(256);
    if (!xhci->event_ring) {
        printk("XHCI: Failed to allocate event ring\n");
        return -1;
    }

    // 分配事件环段表 (ERST)
    xhci->erst = (xhci_erst_entry_t *)xhci_alloc_dma(sizeof(xhci_erst_entry_t), &xhci->erst_phys);
    if (!xhci->erst) {
        printk("XHCI: Failed to allocate ERST\n");
        return -1;
    }

    xhci->erst[0].address  = xhci->event_ring->phys_addr;
    xhci->erst[0].size     = xhci->event_ring->size;
    xhci->erst[0].reserved = 0;

    // 配置中断器0
    xhci_writel(&xhci->intr_regs[0].erstsz, 1);
    xhci_writeq(&xhci->intr_regs[0].erstba, xhci->erst_phys);
    xhci_writeq(&xhci->intr_regs[0].erdp, xhci->event_ring->phys_addr);

    xhci_writel(&xhci->intr_regs[0].imod, 0);

    // // 启用中断
    // uint32_t iman = xhci_readl(&xhci->intr_regs[0].iman);
    // iman |= (1 << 1);
    // xhci_writel(&xhci->intr_regs[0].iman, iman);

    // 分配端口信息数组
    xhci->port_info = (xhci_port_info_t *)malloc(sizeof(xhci_port_info_t) * xhci->max_ports);
    if (!xhci->port_info) {
        printk("XHCI: Failed to allocate port info\n");
        return -1;
    }
    memset(xhci->port_info, 0, sizeof(xhci_port_info_t) * xhci->max_ports);

    // 解析端口协议
    xhci_parse_protocol_caps(xhci);

    // uint32_t cmd = xhci_readl(&xhci->op_regs->usbcmd);
    // cmd |= XHCI_CMD_INTE;
    // xhci_writel(&xhci->op_regs->usbcmd, cmd);

    // 启动控制器
    if (xhci_start(xhci) != 0) { return -1; }

    // 启动事件处理线程
    if (xhci_start_event_handler(xhci) != 0) {
        printk("XHCI: Failed to start event handler\n");
        return -1;
    }

    for (uint8_t p = 0; p < xhci->max_ports; p++) {
        if (xhci_readl(&xhci->port_regs[p].portsc) & XHCI_PORTSC_CCS) {
            xhci_handle_port_status(xhci, p);
        }
    }

    xhci_handle_events(xhci);

    printk("XHCI: Initialization complete\n");
    return 0;
}

// 关闭XHCI HCD
static int xhci_hcd_shutdown(usb_hcd_t *hcd) {
    xhci_hcd_t *xhci = (xhci_hcd_t *)hcd->private_data;

    printk("XHCI: Shutting down controller\n");

    // 停止事件处理线程
    xhci_stop_event_handler(xhci);

    xhci_stop(xhci);

    // // 释放资源
    // if (xhci->cmd_ring) {
    //     xhci_free_ring(xhci->cmd_ring);
    // }

    // if (xhci->event_ring) {
    //     xhci_free_ring(xhci->event_ring);
    // }

    // if (xhci->erst) {
    //     free(xhci->erst);
    // }

    // if (xhci->scratchpad_buffers) {
    //     for (uint32_t i = 0; i < xhci->num_scratchpads; i++) {
    //         if (xhci->scratchpad_buffers[i]) {
    //             free(xhci->scratchpad_buffers[i]);
    //         }
    //     }
    //     free(xhci->scratchpad_buffers);
    // }

    // if (xhci->scratchpad_array) {
    //     free(xhci->scratchpad_array);
    // }

    // if (xhci->dcbaa) {
    //     free(xhci->dcbaa);
    // }

    free(xhci);

    return 0;
}

// 重置端口
static int xhci_reset_port(usb_hcd_t *hcd, uint8_t port) {
    xhci_hcd_t *xhci = (xhci_hcd_t *)hcd->private_data;

    if (port >= xhci->max_ports) { return -1; }

    xhci_port_info_t *port_info = &xhci->port_info[port];

    if (port_info->protocol == XHCI_PROTOCOL_USB3) {
        // 检查端口是否已经在 U0 状态
        uint32_t portsc = xhci_readl(&xhci->port_regs[port].portsc);
        uint32_t pls    = (portsc >> 5) & 0xF; // Port Link State

        printk("  Port Link State: %u\n", pls);

        if (pls == 0) { // U0 - active
            printk("  Port is in U0 (active) - ready for enumeration\n");
        } else {
            printk("  Port not in U0, waiting for link training...\n");

            // 等待链路训练完成
            int timeout = 100;
            while (timeout-- > 0) {
                portsc = xhci_readl(&xhci->port_regs[port].portsc);
                pls    = (portsc >> 5) & 0xF;

                if (pls == 0) { // U0
                    printk("  Link training complete, port in U0\n");
                    break;
                }

                scheduler_yield();
            }

            if (pls != 0) {
                printk("  WARNING: Port not in U0 after timeout (PLS=%u)\n", pls);
                return -1;
            }
        }

        return 0; // 成功，无需重置
    }

    uint32_t portsc = xhci_readl(&xhci->port_regs[port].portsc);

    // 发起端口重置
    portsc |= XHCI_PORTSC_PR;
    xhci_writel(&xhci->port_regs[port].portsc, portsc);

    // 等待重置完成
    int timeout = 1000;
    while (timeout--) {
        portsc = xhci_readl(&xhci->port_regs[port].portsc);
        if (!(portsc & XHCI_PORTSC_CCS)) return -1;
        if (portsc & XHCI_PORTSC_PED) { break; }

        scheduler_yield();
    }

    if (timeout <= 0) {
        printk("XHCI: Port reset timeout\n");
        return -1;
    }

    // 清除端口变化状态位
    portsc  = xhci_readl(&xhci->port_regs[port].portsc);
    portsc |= XHCI_PORTSC_PRC | XHCI_PORTSC_PEC;
    xhci_writel(&xhci->port_regs[port].portsc, portsc);

    printk("XHCI: USB2 port reset complete\n");

    return 0;
}

// 为设备初始化EP0（在enable_slot之后调用）
int xhci_setup_default_endpoint(usb_hcd_t *hcd, usb_device_t *device) {
    // 为EP0分配端点私有数据
    xhci_endpoint_private_t *ep0_priv =
        (xhci_endpoint_private_t *)malloc(sizeof(xhci_endpoint_private_t));
    if (!ep0_priv) {
        printk("XHCI: Failed to allocate EP0 private data\n");
        return -1;
    }

    memset(ep0_priv, 0, sizeof(xhci_endpoint_private_t));
    ep0_priv->dci = 1; // EP0的DCI总是1

    // 分配传输环
    ep0_priv->transfer_ring = xhci_alloc_ring(256);
    if (!ep0_priv->transfer_ring) {
        printk("XHCI: Failed to allocate EP0 transfer ring\n");
        free(ep0_priv);
        return -1;
    }

    // 设置EP0基本信息
    device->endpoints[0].address         = 0;
    device->endpoints[0].attributes      = USB_ENDPOINT_XFER_CONTROL;
    device->endpoints[0].max_packet_size = 8; // 默认最小值，后续会更新
    device->endpoints[0].interval        = 0;
    device->endpoints[0].device          = device;
    device->endpoints[0].hcd_private     = ep0_priv;

    return 0;
}

spin_t xhci_command_lock = {0};

// 启用槽位 - 使用等待机制
static int xhci_enable_slot(usb_hcd_t *hcd, usb_device_t *device) {
    xhci_hcd_t *xhci = (xhci_hcd_t *)hcd->private_data;

    printk("XHCI: Enabling slot\n");

    // 分配设备私有数据
    xhci_device_private_t *dev_priv =
        (xhci_device_private_t *)malloc(sizeof(xhci_device_private_t));
    if (!dev_priv) { return -1; }
    memset(dev_priv, 0, sizeof(xhci_device_private_t));

    // 分配设备上下文
    dev_priv->device_ctx =
        (xhci_device_ctx_t *)xhci_alloc_dma(sizeof(xhci_device_ctx_t), &dev_priv->device_ctx_phys);
    if (!dev_priv->device_ctx) {
        free(dev_priv);
        return -1;
    }

    // 分配输入上下文
    dev_priv->input_ctx =
        (xhci_input_ctx_t *)xhci_alloc_dma(sizeof(xhci_input_ctx_t), &dev_priv->input_ctx_phys);
    if (!dev_priv->input_ctx) {
        free(dev_priv->device_ctx);
        free(dev_priv);
        return -1;
    }

    // 分配完成结构
    xhci_command_completion_t *completion = xhci_alloc_command_completion();
    if (!completion) {
        free(dev_priv->input_ctx);
        free(dev_priv->device_ctx);
        free(dev_priv);
        return -1;
    }

    spin_lock(xhci_command_lock);

    // 发送Enable Slot命令
    xhci_trb_t *cmd_trb =
        xhci_queue_trb(xhci->cmd_ring, 0, 0, (TRB_TYPE_ENABLE_SLOT << 10) | TRB_FLAG_IOC);

    // 跟踪命令
    xhci_track_command(xhci, cmd_trb, completion, TRB_TYPE_ENABLE_SLOT);

    // 敲门铃
    xhci_ring_doorbell(xhci, 0, 0);

    spin_unlock(xhci_command_lock);

    // 等待命令完成（5秒超时）
    int ret = xhci_wait_for_command(completion, 5000);

    if (ret == 0) {
        // 命令成功，获取slot ID
        dev_priv->slot_id = completion->slot_id;

        // 更新DCBAA
        xhci->dcbaa[dev_priv->slot_id] = dev_priv->device_ctx_phys;

        device->hcd_private = dev_priv;

        printk("XHCI: Slot %d enabled\n", dev_priv->slot_id);

        // 初始化EP0
        ret = xhci_setup_default_endpoint(hcd, device);
        if (ret != 0) {
            printk("XHCI: Failed to setup EP0\n");
            xhci->dcbaa[dev_priv->slot_id] = 0;
            free(dev_priv->input_ctx);
            free(dev_priv->device_ctx);
            free(dev_priv);
            device->hcd_private = NULL;
            xhci_free_command_completion(completion);
            return -1;
        }
    } else {
        // 命令失败
        free(dev_priv->input_ctx);
        free(dev_priv->device_ctx);
        free(dev_priv);
    }

    xhci_free_command_completion(completion);
    return ret;
}

// 禁用槽位
static int xhci_disable_slot(usb_hcd_t *hcd, usb_device_t *device) {
    xhci_hcd_t            *xhci     = (xhci_hcd_t *)hcd->private_data;
    xhci_device_private_t *dev_priv = (xhci_device_private_t *)device->hcd_private;

    if (!dev_priv) { return -1; }

    printk("XHCI: Disabling slot %d\n", dev_priv->slot_id);

    spin_lock(xhci_command_lock);

    // 发送Disable Slot命令
    xhci_trb_t *trb = xhci_queue_trb(xhci->cmd_ring, 0, 0,
                                     (TRB_TYPE_DISABLE_SLOT << 10) |
                                         ((uint32_t)dev_priv->slot_id << 24) | TRB_FLAG_IOC);

    xhci_ring_doorbell(xhci, 0, 0);

    spin_unlock(xhci_command_lock);

    xhci_handle_events(xhci);

    // 清除DCBAA
    xhci->dcbaa[dev_priv->slot_id] = 0;

    // 释放资源
    if (dev_priv->device_ctx) { free(dev_priv->device_ctx); }
    if (dev_priv->input_ctx) { free(dev_priv->input_ctx); }
    free(dev_priv);

    device->hcd_private = NULL;

    return 0;
}

// 地址设备 - 这是设备枚举的关键步骤
static int xhci_address_device(usb_hcd_t *hcd, usb_device_t *device, uint8_t address) {
    xhci_hcd_t            *xhci     = (xhci_hcd_t *)hcd->private_data;
    xhci_device_private_t *dev_priv = (xhci_device_private_t *)device->hcd_private;

    if (!dev_priv) {
        printk("XHCI: ERROR - Device private data is NULL\n");
        return -1;
    }

    // 验证EP0已经初始化
    if (!device->endpoints[0].hcd_private) {
        printk("XHCI: CRITICAL - EP0 not initialized!\n");
        return -1;
    }

    xhci_endpoint_private_t *ep0_priv = (xhci_endpoint_private_t *)device->endpoints[0].hcd_private;

    if (!ep0_priv->transfer_ring) {
        printk("XHCI: CRITICAL - EP0 transfer ring not allocated!\n");
        return -1;
    }

    // 清零输入上下文
    memset(dev_priv->input_ctx, 0, sizeof(xhci_input_ctx_t));

    // ===== 1. 设置输入控制上下文 =====
    dev_priv->input_ctx->ctrl.drop_flags = 0;
    dev_priv->input_ctx->ctrl.add_flags  = 0x03; // A0 (Slot) | A1 (EP0)

    // ===== 2. 配置 Slot 上下文 =====
    uint32_t route_string    = 0; // Root hub port
    uint32_t speed           = device->speed;
    uint32_t context_entries = 1; // EP0的DCI是1

    // dev_info: Route String, Speed, Context Entries
    dev_priv->input_ctx->slot.dev_info = SLOT_CTX_ROUTE_STRING(route_string) |
                                         SLOT_CTX_SPEED(speed) |
                                         SLOT_CTX_CONTEXT_ENTRIES(context_entries);

    // dev_info2: Root Hub Port Number
    // 注意：如果设备连接在root hub的port N，这里应该设置为N+1
    uint8_t root_port                   = device->port + 1;
    dev_priv->input_ctx->slot.dev_info2 = SLOT_CTX_ROOT_HUB_PORT(root_port);

    // tt_info: TT相关，对于高速设备为0
    dev_priv->input_ctx->slot.tt_info = 0;

    // dev_state: 由硬件设置
    dev_priv->input_ctx->slot.dev_state = 0;

    // ===== 3. 配置 EP0 上下文 =====

    // 根据速度确定最大包大小
    uint16_t max_packet_size;
    switch (speed) {
    case USB_SPEED_LOW: max_packet_size = 8; break;
    case USB_SPEED_FULL:
        max_packet_size = 8; // 初始值，后续可能更新为64
        break;
    case USB_SPEED_HIGH: max_packet_size = 64; break;
    case USB_SPEED_SUPER:
    case USB_SPEED_SUPER + 1: // Super Speed Plus
        max_packet_size = 512;
        break;
    default: max_packet_size = 8; break;
    }

    // ep_info: EP State, Interval, etc.
    // Control endpoints 没有 interval，设为0
    dev_priv->input_ctx->endpoints[0].ep_info = 0;

    // ep_info2: Error Count, EP Type, Max Burst, Max Packet Size
    dev_priv->input_ctx->endpoints[0].ep_info2 =
        EP_CTX_ERROR_COUNT(3) |           // CErr = 3
        EP_CTX_EP_TYPE(EP_TYPE_CONTROL) | // Control endpoint
        EP_CTX_MAX_BURST(0) |             // Max Burst = 0 for control
        EP_CTX_MAX_PACKET_SIZE(max_packet_size);

    // tr_dequeue_ptr: Transfer Ring地址 + DCS bit
    dev_priv->input_ctx->endpoints[0].tr_dequeue_ptr =
        ep0_priv->transfer_ring->phys_addr | EP_CTX_DCS;

    // tx_info: Average TRB Length
    // 对于控制端点，设置为8（setup packet大小）
    dev_priv->input_ctx->endpoints[0].tx_info = 8;

    // 更新设备端点信息
    device->endpoints[0].max_packet_size = max_packet_size;

    // 验证输入上下文物理地址对齐
    if (dev_priv->input_ctx_phys & 0x3F) {
        printk("XHCI: WARNING - Input context not 64-byte aligned: 0x%lx\n",
               dev_priv->input_ctx_phys);
    }

    // ===== 4. 发送 Address Device 命令 =====

    xhci_command_completion_t *completion = xhci_alloc_command_completion();
    if (!completion) {
        printk("XHCI: Failed to allocate command completion\n");
        return -1;
    }

    spin_lock(xhci_command_lock);

    // Address Device TRB:
    // - Parameter: Input Context Pointer
    // - Status: Reserved (0)
    // - Control: TRB Type, Slot ID, BSR (Block Set Address Request)

    // BSR=0 表示要真正分配地址（如果BSR=1，只是启用slot但不分配地址）
    uint32_t control = (TRB_TYPE_ADDRESS_DEV << 10) |        // TRB Type
                       ((uint32_t)dev_priv->slot_id << 24) | // Slot ID
                       (0 << 9) |                            // BSR = 0 (分配地址)
                       TRB_FLAG_IOC;                         // Interrupt on Completion

    xhci_trb_t *cmd_trb = xhci_queue_trb(xhci->cmd_ring,
                                         dev_priv->input_ctx_phys, // Input Context pointer
                                         0,                        // Status (reserved)
                                         control);

    xhci_track_command(xhci, cmd_trb, completion, TRB_TYPE_ADDRESS_DEV);

    // 敲门铃
    xhci_ring_doorbell(xhci, 0, 0);

    spin_unlock(xhci_command_lock);

    // 等待命令完成
    int ret = xhci_wait_for_command(completion, 5000);

    if (ret == 0) {
        device->address = address;
        device->state   = USB_STATE_ADDRESS;
    } else {
        printk("\n*** Address Device FAILED ***\n");
        printk("Error Code: %d\n", ret);
        printk("Completion Code: %d\n", completion->completion_code);
    }

    xhci_free_command_completion(completion);
    return ret;
}

static int xhci_configure_endpoint(usb_hcd_t *hcd, usb_endpoint_t *endpoint) {
    xhci_hcd_t            *xhci     = (xhci_hcd_t *)hcd->private_data;
    usb_device_t          *device   = endpoint->device;
    xhci_device_private_t *dev_priv = (xhci_device_private_t *)device->hcd_private;

    if (!dev_priv) {
        printk("XHCI: Device private data is NULL\n");
        return -1;
    }

    uint8_t ep_num = endpoint->address & 0x0F;
    uint8_t is_in  = (endpoint->address & 0x80) ? 1 : 0;

    // 计算 DCI (Device Context Index)
    // EP0 的 DCI 是 1
    // 其他端点：OUT 端点 DCI = ep_num * 2, IN 端点 DCI = ep_num * 2 + 1
    uint8_t dci;
    if (ep_num == 0) {
        dci = 1; // EP0
    } else {
        dci = (ep_num * 2) + (is_in ? 1 : 0);
    }

    // 分配端点私有数据
    xhci_endpoint_private_t *ep_priv =
        (xhci_endpoint_private_t *)malloc(sizeof(xhci_endpoint_private_t));
    if (!ep_priv) {
        printk("XHCI: Failed to allocate endpoint private data\n");
        return -1;
    }

    memset(ep_priv, 0, sizeof(xhci_endpoint_private_t));
    ep_priv->dci = dci;

    // 分配传输环
    ep_priv->transfer_ring = xhci_alloc_ring(256);
    if (!ep_priv->transfer_ring) {
        printk("XHCI: Failed to allocate transfer ring\n");
        free(ep_priv);
        return -1;
    }

    // 清空输入上下文
    memset(dev_priv->input_ctx, 0, sizeof(xhci_input_ctx_t));

    // 设置输入控制上下文
    // A0 (Slot Context) 必须设置，因为我们要更新 Context Entries
    // A[DCI] 设置我们要配置的端点
    dev_priv->input_ctx->ctrl.add_flags  = (1 << dci) | (1 << 0); // A[DCI] | A0
    dev_priv->input_ctx->ctrl.drop_flags = 0;

    // 更新 Slot Context
    // Context Entries 必须 >= DCI（包含所有活动端点）
    uint32_t context_entries = dci; // 至少要包含这个端点

    // 保留原有的 route string 和 speed
    uint32_t route_string = dev_priv->input_ctx->slot.dev_info & 0xFFFFF;
    uint32_t speed        = (dev_priv->input_ctx->slot.dev_info >> 20) & 0xF;

    // 如果这些值为0（没有初始化），使用设备的值
    if (speed == 0) { speed = device->speed; }

    dev_priv->input_ctx->slot.dev_info = SLOT_CTX_ROUTE_STRING(route_string) |
                                         SLOT_CTX_SPEED(speed) |
                                         SLOT_CTX_CONTEXT_ENTRIES(context_entries);

    // 保留 Root Hub Port Number
    uint32_t root_port = (dev_priv->input_ctx->slot.dev_info2 >> 16) & 0xFF;
    if (root_port == 0) { root_port = device->port + 1; }

    dev_priv->input_ctx->slot.dev_info2 = SLOT_CTX_ROOT_HUB_PORT(root_port);

    // 配置端点上下文

    uint32_t ep_type_attr = endpoint->attributes & 0x03;
    uint32_t xhci_ep_type;

    // 确定 XHCI EP Type
    switch (ep_type_attr) {
    case USB_ENDPOINT_XFER_CONTROL:
        xhci_ep_type = EP_TYPE_CONTROL; // 4
        break;
    case USB_ENDPOINT_XFER_ISOC:
        xhci_ep_type = is_in ? EP_TYPE_ISOC_IN : EP_TYPE_ISOC_OUT; // 5 : 1
        break;
    case USB_ENDPOINT_XFER_BULK:
        xhci_ep_type = is_in ? EP_TYPE_BULK_IN : EP_TYPE_BULK_OUT; // 6 : 2
        break;
    case USB_ENDPOINT_XFER_INT:
        xhci_ep_type = is_in ? EP_TYPE_INTERRUPT_IN : EP_TYPE_INTERRUPT_OUT; // 7 : 3
        break;
    default:
        printk("XHCI: Unknown endpoint type: 0x%02x\n", ep_type_attr);
        xhci_free_ring(ep_priv->transfer_ring);
        free(ep_priv);
        return -1;
    }

    // 计算 Interval（对于中断和等时端点）
    uint32_t interval = 0;
    if (ep_type_attr == USB_ENDPOINT_XFER_INT || ep_type_attr == USB_ENDPOINT_XFER_ISOC) {

        // 将 USB bInterval 转换为 XHCI Interval
        // XHCI Interval = log2(bInterval) for HS/SS
        // 对于 FS/LS 中断端点，转换公式不同

        uint8_t bInterval = endpoint->interval;

        if (speed == USB_SPEED_HIGH || speed == USB_SPEED_SUPER) {
            // 对于高速/超速，Interval = bInterval - 1
            if (bInterval > 0) { interval = bInterval - 1; }
        } else {
            // 对于全速/低速，需要转换
            // bInterval 是毫秒数，需要转换为 2^n 微帧
            if (bInterval > 0) {
                // 简化：取 log2
                uint32_t val = bInterval;
                interval     = 0;
                while (val > 1) {
                    val >>= 1;
                    interval++;
                }
                interval += 3; // 转换到微帧（8微帧 = 1毫秒）
            }
        }

        // 限制在有效范围内 (0-15)
        if (interval > 15) { interval = 15; }
    }

    // ep_info: Interval, MaxPStreams, Mult, etc.
    dev_priv->input_ctx->endpoints[dci - 1].ep_info =
        EP_CTX_INTERVAL(interval) | EP_CTX_MAX_P_STREAMS(0) | // 0 = Linear Stream Array
        EP_CTX_MULT(0);                                       // 0 for non-SS, not isoc

    // ep_info2: Error Count, EP Type, Max Burst Size, Max Packet Size
    dev_priv->input_ctx->endpoints[dci - 1].ep_info2 =
        EP_CTX_ERROR_COUNT(3) |        // CErr = 3 (retry count)
        EP_CTX_EP_TYPE(xhci_ep_type) | // EP Type
        EP_CTX_MAX_BURST(0) |          // 0 for non-SS endpoints
        EP_CTX_MAX_PACKET_SIZE(endpoint->max_packet_size);

    // tr_dequeue_ptr: Transfer Ring 物理地址 + DCS
    dev_priv->input_ctx->endpoints[dci - 1].tr_dequeue_ptr =
        ep_priv->transfer_ring->phys_addr | EP_CTX_DCS;

    // tx_info: Average TRB Length (用于带宽计算)
    // 对于批量端点，设置为典型值或 max packet size
    uint32_t avg_trb_length = endpoint->max_packet_size;
    if (ep_type_attr == USB_ENDPOINT_XFER_CONTROL) {
        avg_trb_length = 8; // Control setup packet
    }

    dev_priv->input_ctx->endpoints[dci - 1].tx_info = avg_trb_length;

    // 发送 Configure Endpoint 命令

    xhci_command_completion_t *completion = xhci_alloc_command_completion();
    if (!completion) {
        printk("XHCI: Failed to allocate command completion\n");
        xhci_free_ring(ep_priv->transfer_ring);
        free(ep_priv);
        return -1;
    }

    spin_lock(xhci_command_lock);

    // Configure Endpoint TRB
    xhci_trb_t *cmd_trb = xhci_queue_trb(xhci->cmd_ring, dev_priv->input_ctx_phys, 0,
                                         (TRB_TYPE_CONFIG_EP << 10) |
                                             ((uint32_t)dev_priv->slot_id << 24) | TRB_FLAG_IOC);

    xhci_track_command(xhci, cmd_trb, completion, TRB_TYPE_CONFIG_EP);

    xhci_ring_doorbell(xhci, 0, 0);

    spin_unlock(xhci_command_lock);

    // 等待命令完成
    int ret = xhci_wait_for_command(completion, 5000);

    if (ret == 0) {
        endpoint->hcd_private = ep_priv;
        printk("XHCI: Endpoint configured successfully\n");
        printk("  DCI: %d\n", dci);
        printk("  Transfer Ring: %p\n", ep_priv->transfer_ring);
    } else {
        printk("XHCI: Configure Endpoint failed\n");
        xhci_free_ring(ep_priv->transfer_ring);
        free(ep_priv);
    }

    xhci_free_command_completion(completion);
    return ret;
}

spin_t xhci_transfer_lock = {0};

// 控制传输 - 使用等待机制
static int xhci_control_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer,
                                 usb_device_request_t *setup) {
    xhci_hcd_t            *xhci     = (xhci_hcd_t *)hcd->private_data;
    usb_device_t          *device   = transfer->device;
    xhci_device_private_t *dev_priv = (xhci_device_private_t *)device->hcd_private;

    if (!dev_priv) { return -1; }

    uint8_t                  dci     = 1;
    xhci_endpoint_private_t *ep_priv = (xhci_endpoint_private_t *)device->endpoints[0].hcd_private;

    if (!ep_priv) { return -1; }

    xhci_ring_t *ring = ep_priv->transfer_ring;

    // 分配完成结构
    xhci_transfer_completion_t *completion = xhci_alloc_transfer_completion();
    if (!completion) { return -1; }

    spin_lock(xhci_transfer_lock);

    // Setup Stage TRB
    uint64_t setup_data = *(uint64_t *)setup;

    xhci_trb_t *first_trb =
        xhci_queue_trb(ring, setup_data, 8,
                       (TRB_TYPE_SETUP << 10) | TRB_FLAG_IDT |
                           ((setup->bmRequestType & USB_DIR_IN) ? (3 << 16) : (0 << 16)));

    // Data Stage TRB
    if (transfer->length > 0 && transfer->buffer) {
        uint64_t data_addr = page_virt_to_phys((uint64_t)transfer->buffer);
        uint32_t direction = (setup->bmRequestType & USB_DIR_IN) ? (1 << 16) : 0;
        xhci_queue_trb(ring, data_addr, transfer->length, (TRB_TYPE_DATA << 10) | direction);
    }

    // Status Stage TRB
    uint32_t status_dir = (setup->bmRequestType & USB_DIR_IN) ? 0 : (1 << 16);
    if (transfer->length == 0) { status_dir = (setup->bmRequestType & USB_DIR_IN) ? (1 << 16) : 0; }

    xhci_queue_trb(ring, 0, 0, (TRB_TYPE_STATUS << 10) | status_dir | TRB_FLAG_IOC);

    uint8_t first_index = first_trb - ring->trbs;

    // 跟踪传输
    xhci_track_transfer(xhci, first_trb,
                        xhci_calc_trbs_num(ring->enqueue_index, first_index, ring->size),
                        completion, transfer);

    // 敲门铃
    xhci_ring_doorbell(xhci, dev_priv->slot_id, dci);

    spin_unlock(xhci_transfer_lock);

    // 等待传输完成（5秒超时）
    int ret = xhci_wait_for_transfer(completion, 5000);

    if (ret >= 0) {
        transfer->status        = 0;
        transfer->actual_length = completion->transferred_length;
    } else {
        transfer->status        = ret;
        transfer->actual_length = 0;
    }

    xhci_free_transfer_completion(completion);

    return ret;
}

// 批量传输 - 使用等待机制
static int xhci_bulk_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer) {
    xhci_hcd_t              *xhci     = (xhci_hcd_t *)hcd->private_data;
    usb_device_t            *device   = transfer->device;
    xhci_device_private_t   *dev_priv = (xhci_device_private_t *)device->hcd_private;
    xhci_endpoint_private_t *ep_priv  = (xhci_endpoint_private_t *)transfer->endpoint->hcd_private;

    if (!dev_priv || !ep_priv) { return -1; }

    xhci_ring_t *ring = ep_priv->transfer_ring;

    // 分配完成结构
    xhci_transfer_completion_t *completion = xhci_alloc_transfer_completion();
    if (!completion) { return -1; }
    completion->transfer_type = NORMAL_TRANSFER;

    spin_lock(xhci_transfer_lock);

    // 获取第一个TRB指针
    xhci_trb_t *first_trb = NULL;

    uint16_t max_packet_size = transfer->endpoint->max_packet_size;

    size_t td_packet_count = (transfer->length + max_packet_size - 1) / max_packet_size;

    size_t progress = 0;

    while (progress < transfer->length) {
        uint64_t data_addr = page_virt_to_phys((uint64_t)transfer->buffer + progress);

        size_t chunk = MIN(transfer->length - progress, PAGE_SIZE - (data_addr & 0xFFF));

        bool chain = (progress + chunk) < transfer->length;

        size_t td_size = td_packet_count - (progress + chunk) / max_packet_size;
        if ((progress + chunk) == transfer->length) td_size = 0;

        // Normal TRB
        if (td_size > 31) td_size = 31;
        xhci_trb_t *trb =
            xhci_queue_trb(ring, data_addr, chunk | (td_size << 17),
                           (TRB_TYPE_NORMAL << 10) | (chain ? TRB_FLAG_CHAIN : TRB_FLAG_IOC));

        if (!first_trb) { first_trb = trb; }

        progress += chunk;
    }

    uint8_t first_index = first_trb - ring->trbs;

    // 跟踪传输
    xhci_track_transfer(xhci, first_trb,
                        xhci_calc_trbs_num(ring->enqueue_index, first_index, ring->size),
                        completion, transfer);

    // 敲门铃
    xhci_ring_doorbell(xhci, dev_priv->slot_id, ep_priv->dci);

    spin_unlock(xhci_transfer_lock);

    // 等待传输完成
    int ret = xhci_wait_for_transfer(completion, 5000);

    if (ret >= 0) {
        transfer->status        = 0;
        transfer->actual_length = completion->transferred_length;
    } else {
        transfer->status        = ret;
        transfer->actual_length = 0;
    }

    xhci_free_transfer_completion(completion);

    return ret;
}

// 中断传输 - 使用需放到单独线程
static int xhci_interrupt_transfer(usb_hcd_t *hcd, usb_transfer_t *transfer) {
    xhci_hcd_t              *xhci     = (xhci_hcd_t *)hcd->private_data;
    usb_device_t            *device   = transfer->device;
    xhci_device_private_t   *dev_priv = (xhci_device_private_t *)device->hcd_private;
    xhci_endpoint_private_t *ep_priv  = (xhci_endpoint_private_t *)transfer->endpoint->hcd_private;

    if (!dev_priv || !ep_priv) { return -1; }

    xhci_ring_t *ring = ep_priv->transfer_ring;

    // 分配完成结构
    xhci_transfer_completion_t *completion = xhci_alloc_transfer_completion();
    if (!completion) { return -1; }
    completion->transfer_type = INTR_TRANSFER;

    spin_lock(xhci_transfer_lock);

    // Normal TRB
    uint64_t data_addr = page_virt_to_phys((uint64_t)transfer->buffer);

    uint32_t control = (TRB_TYPE_NORMAL << 10) | TRB_FLAG_IOC;

    xhci_trb_t *first_trb = xhci_queue_trb(ring, data_addr, transfer->length, control);

    uint8_t first_index = first_trb - ring->trbs;

    // 跟踪传输
    xhci_track_transfer(xhci, first_trb,
                        xhci_calc_trbs_num(ring->enqueue_index, first_index, ring->size),
                        completion, transfer);

    // 敲门铃
    xhci_ring_doorbell(xhci, dev_priv->slot_id, ep_priv->dci);

    spin_unlock(xhci_transfer_lock);

    int ret = xhci_wait_for_transfer(completion, 0);

    if (ret >= 0) {
        transfer->status        = 0;
        transfer->actual_length = completion->transferred_length;
    } else {
        transfer->status        = ret;
        transfer->actual_length = 0;
    }

    xhci_free_transfer_completion(completion);

    return 0;
}

// 从端口状态获取设备速度
static uint8_t xhci_port_speed_to_usb_speed(uint32_t portsc) {
    // XHCI PORTSC 的速度字段在 bits 10-13
    uint32_t port_speed = (portsc >> 10) & 0x0F;

    switch (port_speed) {
    case 1: return USB_SPEED_FULL;  // Full Speed (12 Mbps)
    case 2: return USB_SPEED_LOW;   // Low Speed (1.5 Mbps)
    case 3: return USB_SPEED_HIGH;  // High Speed (480 Mbps)
    case 4: return USB_SPEED_SUPER; // Super Speed (5 Gbps)
    default: return USB_SPEED_UNKNOWN;
    }
}

// 获取端口速度名称
static const char *usb_speed_name(uint8_t speed) {
    switch (speed) {
    case USB_SPEED_LOW: return "Low Speed (1.5 Mbps)";
    case USB_SPEED_FULL: return "Full Speed (12 Mbps)";
    case USB_SPEED_HIGH: return "High Speed (480 Mbps)";
    case USB_SPEED_SUPER: return "Super Speed (5 Gbps)";
    default: return "Unknown Speed";
    }
}

typedef struct enumerater_arg {
    usb_hcd_t *hcd;
    uint8_t    port_id;
    uint8_t    speed;
} enumerater_arg_t;

spin_t enumerate_lock = {0};

void xhci_device_enumerater(enumerater_arg_t *arg) {
    spin_lock(enumerate_lock);
    usb_hcd_t *hcd = arg->hcd;
    if (hcd) {
        int ret = 0; //TODO usb_enumerate_device(hcd, arg->port_id, arg->speed);
        if (ret == 1) {
            printk("XHCI: Device on port %d enumerated successfully\n", arg->port_id);
        } else {
            printk("XHCI: Failed to enumerate device on port %d\n", arg->port_id);
        }
    }
    free(arg);
    spin_unlock(enumerate_lock);
    task_exit(0);
}

// 处理端口状态变化
void xhci_handle_port_status(xhci_hcd_t *xhci, uint8_t port_id) {
    if (port_id >= xhci->max_ports) { return; }

    uint32_t portsc = xhci_readl(&xhci->port_regs[port_id].portsc);

    // printk("XHCI: Port %d status: 0x%08x\n", port_id, portsc);

    // 连接状态变化
    if ((portsc & XHCI_PORTSC_CCS) && !xhci->connection[port_id]) {
        // 设备连接
        printk("XHCI: Device connected on port %d\n", port_id);

        // 获取设备速度
        uint8_t speed = xhci_port_speed_to_usb_speed(portsc);
        printk("XHCI: Device speed: %s\n", usb_speed_name(speed));

        // 等待端口稳定
        uint64_t target_time = 100000000ULL + nano_time(); // 100ms
        while (nano_time() < target_time) {
            scheduler_yield();
        }

        // 启动设备枚举
        printk("XHCI: Starting device enumeration...\n");

        xhci->connection[port_id] = true;

        enumerater_arg_t *arg = malloc(sizeof(enumerater_arg_t));
        arg->hcd              = xhci->hcd;
        arg->port_id          = port_id;
        arg->speed            = speed;

        task_create("enumerater", (void (*)(uint64_t))xhci_device_enumerater, (uint64_t)arg,
                    KTHREAD_PRIORITY);

        __asm__ volatile("sti");
    } else if (!(portsc & XHCI_PORTSC_CCS)) {
        printk("XHCI: Device disconnected on port %d\n", port_id);
        xhci->connection[port_id] = false;
    }

    // 清除CSC
    xhci_writel(&xhci->port_regs[port_id].portsc, portsc | XHCI_PORTSC_CSC);

    if (portsc & XHCI_PORTSC_PEC) {
        // 端口使能变化
        printk("XHCI: Port %d enable changed\n", port_id);
        xhci_writel(&xhci->port_regs[port_id].portsc, portsc | XHCI_PORTSC_PEC);
    }

    // if (portsc & XHCI_PORTSC_PRC) {
    //     // 端口重置完成
    //     printk("XHCI: Port %d reset complete\n", port_id);
    //     xhci_writel(&xhci->port_regs[port_id].portsc, portsc |
    //     XHCI_PORTSC_PRC);
    // }
}

// 初始化XHCI驱动
usb_hcd_t *xhci_init(void *mmio_base, pci_device_t *pci_dev) {
    // 分配XHCI上下文
    xhci_hcd_t *xhci = (xhci_hcd_t *)malloc(sizeof(xhci_hcd_t));
    if (!xhci) {
        printk("XHCI: Failed to allocate context\n");
        return NULL;
    }
    memset(xhci, 0, sizeof(xhci_hcd_t));

    // 初始化寄存器指针
    xhci->cap_regs = (xhci_cap_regs_t *)mmio_base;

    uint8_t  caplength = xhci->cap_regs->caplength;
    uint32_t rtsoff    = xhci_readl(&xhci->cap_regs->rtsoff);
    uint32_t dboff     = xhci_readl(&xhci->cap_regs->dboff);

    xhci->op_regs       = (xhci_op_regs_t *)((uint8_t *)mmio_base + caplength);
    xhci->runtime_regs  = (xhci_runtime_regs_t *)((uint8_t *)mmio_base + rtsoff);
    xhci->doorbell_regs = (uint32_t *)((uint8_t *)mmio_base + dboff);

    xhci->port_regs = (xhci_port_regs_t *)((uint8_t *)xhci->op_regs + 0x400);
    xhci->intr_regs = (xhci_intr_regs_t *)((uint8_t *)xhci->runtime_regs + 0x20);

    memset(xhci->connection, 0, sizeof(xhci->connection));

    xhci->pci_dev = pci_dev;

    // 注册HCD
    usb_hcd_t *hcd = usb_register_hcd("xhci", &xhci_ops, mmio_base, xhci);
    if (!hcd) {
        printk("XHCI: Failed to register HCD\n");
        free(xhci);
        return NULL;
    }

    return hcd;
}

// 关闭XHCI驱动
void xhci_shutdown(usb_hcd_t *hcd) {
    if (hcd) { usb_unregister_hcd(hcd); }
}
//void xhci_hcd_driver_remove(pci_device_t *dev) { xhci_shutdown(dev->desc); }

//oid xhci_hcd_driver_shutdown(pci_device_t *dev) { xhci_shutdown(dev->desc); }

__attribute__((visibility("default"))) int dlmain() {
    pci_device_t *device = pci_find_class(0x000C0330);
    if (device == NULL) {
        logkf("xhci: cannot find xhci controller.\n");
        return -ENODEV;
    }
    uint64_t mmio_base = 0;
    uint64_t mmio_size = 0;
    for (int i = 0; i < 6; i++) {
        if (device->bars[i].size > 0 && device->bars[i].mmio) {
            mmio_base = device->bars[i].address;
            mmio_size = device->bars[i].size;
            break;
        }
    }

    if (mmio_base == 0) {
        printk("xhci: No MMIO BAR found\n");
        return -1;
    }

    void *mmio_vaddr = (void *)phys_to_virt(mmio_base);
    page_map_range(get_current_directory(), (uint64_t)mmio_vaddr, mmio_base, mmio_size,
                   PTE_PRESENT | PTE_WRITEABLE | PTE_DIS_CACHE);
    usb_hcd_t *xhci_hcd = xhci_init(mmio_vaddr, device);
    printk("xhci: %s inited - port:%d\n", xhci_hcd->name, xhci_hcd->devices->port);
    return EOK;
}
