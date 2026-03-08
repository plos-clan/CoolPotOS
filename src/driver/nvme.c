#include "driver/nvme.h"
#include "driver/blk_device.h"
#include "driver/pci/msi.h"
#include "intctl.h"
#include "krlibc.h"
#include "lib/sprintf.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/smp.h"
#include "term/klog.h"
#include "timer.h"

void nvme_interrupt_handler(uint64_t irq_num, void *data, struct pt_regs *r);

// Memory allocation (DMA-capable)
void *cpkrnl_dma_alloc(size_t size, uint64_t *phys_addr) {
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t phys    = alloc_frames(num_pages);
    void *addr       = driver_phys_to_virt(phys);
    page_map_range(get_current_directory(), (uint64_t)addr, phys, size, KERNEL_PTE_FLAGS);
    if (addr) {
        if (phys_addr)
            *phys_addr = driver_virt_to_phys(addr);
        memset(addr, 0, size);
    }
    return addr;
}

static void cpkrnl_dma_free(void *virt, const size_t size) {
    unmap_page_range(get_current_directory(), (uint64_t)virt, size);
}

static void naos_memory_barrier(void) {
}
static void naos_read_barrier(void) {
}
static void naos_write_barrier(void) {
}

void naos_udelay(uint32_t us) {
    uint64_t ns = nano_time() + (uint64_t)us * 1000;
    while (nano_time() < ns) {
        arch_pause();
    }
}
uint64_t naos_get_time_ms(void) {
    return nano_time() / 1000000;
}

// Locking (for multi-threaded environments)
void *naos_mutex_create(void) {
    return NULL;
}
void naos_mutex_lock(void *mutex) {
}
void naos_mutex_unlock(void *mutex) {
}
void naos_mutex_destroy(void *mutex) {
}

int naos_printk(const char *fmt, ...) {
    char buf[2048];
    memset(buf, 0, sizeof(buf));
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printk(buf);
    return n;
}

nvme_platform_ops_t nvme_platform_ops = {
    .dma_alloc     = cpkrnl_dma_alloc,
    .dma_free      = cpkrnl_dma_free,
    .mb            = naos_memory_barrier,
    .rmb           = naos_read_barrier,
    .wmb           = naos_write_barrier,
    .udelay        = naos_udelay,
    .get_time_ms   = naos_get_time_ms,
    .mutex_create  = naos_mutex_create,
    .mutex_lock    = naos_mutex_lock,
    .mutex_unlock  = naos_mutex_unlock,
    .mutex_destroy = naos_mutex_destroy,
    .log           = naos_printk,
};

nvme_platform_ops_t *g_nvme_platform_ops = NULL;

// Helper macros with validation
#define NVME_READ32(ctrl, offset) (*(volatile uint32_t *)((ctrl)->bar0 + (offset)))

#define NVME_WRITE32(ctrl, offset, value)                                                          \
    do {                                                                                           \
        *(volatile uint32_t *)((ctrl)->bar0 + (offset)) = (value);                                 \
        g_nvme_platform_ops->mb();                                                                 \
    } while (0)

#define NVME_READ64(ctrl, offset) (*(volatile uint64_t *)((ctrl)->bar0 + (offset)))

#define NVME_WRITE64(ctrl, offset, value)                                                          \
    do {                                                                                           \
        *(volatile uint64_t *)((ctrl)->bar0 + (offset)) = (value);                                 \
        g_nvme_platform_ops->mb();                                                                 \
    } while (0)

void nvme_set_platform_ops(nvme_platform_ops_t *ops) {
    g_nvme_platform_ops = ops;
}

// Dump controller status for debugging
static void nvme_dump_status(nvme_controller_t *ctrl) {
    uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);
    uint32_t cc   = NVME_READ32(ctrl, NVME_REG_CC);

    g_nvme_platform_ops->log("NVMe: CSTS=0x%08x CC=0x%08x\n", csts, cc);
    g_nvme_platform_ops->log(
        "  RDY=%d CFS=%d SHST=%d NSSRO=%d\n",
        !!(csts & NVME_CSTS_RDY),
        !!(csts & NVME_CSTS_CFS),
        (csts >> 2) & 0x3,
        !!(csts & (1 << 4))
    );
    g_nvme_platform_ops->log(
        "  EN=%d CSS=%d MPS=%d AMS=%d SHN=%d IOSQES=%d IOCQES=%d\n",
        !!(cc & NVME_CC_ENABLE),
        (cc >> 4) & 0x7,
        (cc >> 7) & 0xF,
        (cc >> 11) & 0x7,
        (cc >> 14) & 0x3,
        (cc >> 16) & 0xF,
        (cc >> 20) & 0xF
    );
}

// Enhanced wait for ready with better error reporting
static int nvme_wait_ready(nvme_controller_t *ctrl, bool ready, uint32_t timeout_ms) {
    uint64_t start     = g_nvme_platform_ops->get_time_ms();
    uint32_t last_csts = 0;

    while (1) {
        uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);

        // Check for fatal status first
        if (csts & NVME_CSTS_CFS) {
            g_nvme_platform_ops->log("NVMe: Controller Fatal Status detected!\n");
            nvme_dump_status(ctrl);
            return -1;
        }

        bool is_ready = (csts & NVME_CSTS_RDY) != 0;

        if (is_ready == ready) {
            g_nvme_platform_ops->log("NVMe: Controller ready state reached: %d\n", ready);
            return 0;
        }

        // Log status changes
        if (csts != last_csts) {
            g_nvme_platform_ops->log("NVMe: CSTS changed: 0x%08x -> 0x%08x\n", last_csts, csts);
            last_csts = csts;
        }

        if (g_nvme_platform_ops->get_time_ms() - start > timeout_ms) {
            g_nvme_platform_ops->log("NVMe: Timeout waiting for ready=%d\n", ready);
            nvme_dump_status(ctrl);
            return -1;
        }

        g_nvme_platform_ops->udelay(100);
    }
}

// Enhanced controller reset
static int nvme_reset_controller(nvme_controller_t *ctrl) {
    g_nvme_platform_ops->log("NVMe: Resetting controller...\n");

    // Read current state
    uint32_t cc   = NVME_READ32(ctrl, NVME_REG_CC);
    uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);

    g_nvme_platform_ops->log("NVMe: Initial state - CC=0x%08x CSTS=0x%08x\n", cc, csts);

    // If controller is enabled, disable it
    if (cc & NVME_CC_ENABLE) {
        g_nvme_platform_ops->log("NVMe: Controller is enabled, disabling...\n");

        // Clear enable bit
        cc &= ~NVME_CC_ENABLE;
        NVME_WRITE32(ctrl, NVME_REG_CC, cc);

        // Wait for ready to clear
        if (nvme_wait_ready(ctrl, false, 5000) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to disable controller\n");
            return -1;
        }
    } else {
        // Even if not enabled, ensure RDY is clear
        if (csts & NVME_CSTS_RDY) {
            g_nvme_platform_ops->log("NVMe: Warning - EN=0 but RDY=1, waiting...\n");
            if (nvme_wait_ready(ctrl, false, 5000) != 0) {
                return -1;
            }
        }
    }

    g_nvme_platform_ops->log("NVMe: Controller disabled successfully\n");
    return 0;
}

// Disable controller
static int nvme_disable_controller(nvme_controller_t *ctrl) {
    return nvme_reset_controller(ctrl);
}

// Enable controller with proper configuration
static int nvme_enable_controller(nvme_controller_t *ctrl) {
    g_nvme_platform_ops->log("NVMe: Enabling controller...\n");

    // Ensure controller is disabled first
    uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);
    if (csts & NVME_CSTS_RDY) {
        g_nvme_platform_ops->log("NVMe: Controller still ready, cannot enable\n");
        return -1;
    }

    // Calculate MPS (Memory Page Size)
    // Use host page size (typically 4KB = 2^12, so MPS = 0)
    // MPS value = (log2(page_size) - 12)
    uint32_t mps = 0; // 4KB pages

    // Build CC register value
    uint32_t cc = 0;
    cc |= NVME_CC_ENABLE;             // Enable
    cc |= NVME_CC_CSS_NVM;            // NVM command set
    cc |= (mps << NVME_CC_MPS_SHIFT); // Memory page size
    cc |= NVME_CC_AMS_RR;             // Arbitration: Round Robin
    cc |= NVME_CC_SHN_NONE;           // No shutdown notification
    cc |= NVME_CC_IOSQES;             // I/O Submission Queue Entry Size (64 bytes)
    cc |= NVME_CC_IOCQES;             // I/O Completion Queue Entry Size (16 bytes)

    g_nvme_platform_ops->log("NVMe: Writing CC=0x%08x\n", cc);
    NVME_WRITE32(ctrl, NVME_REG_CC, cc);

    // Verify write
    uint32_t cc_read = NVME_READ32(ctrl, NVME_REG_CC);
    if (cc_read != cc) {
        g_nvme_platform_ops->log(
            "NVMe: CC register write failed! Expected 0x%08x, got 0x%08x\n", cc, cc_read
        );
        return -1;
    }

    // Wait for ready
    g_nvme_platform_ops->log("NVMe: Waiting for controller ready...\n");
    return nvme_wait_ready(ctrl, true, 10000); // Increased timeout to 10s
}

// 为队列绑定中断向量
static int
nvme_bind_queue_interrupt(nvme_controller_t *ctrl, nvme_queue_t *queue, uint16_t vector) {
    logkf("nvme: bind queue int irq:%llu vector:%llu\n\r", vector, vector + IRQ_BASE_VECTOR);
    // 注册中断处理程序
#if defined(__x86_64__)
    uint64_t cpu_id = queue->queue_id ? (queue->queue_id - 1) : 0;
    struct msi_desc_t desc;
    memset(&desc, 0, sizeof(struct msi_desc_t));
    desc.irq_num                    = vector + IRQ_BASE_VECTOR;
    desc.processor                  = cpu_id;
    desc.edge_trigger               = 0;
    desc.assert                     = 1;
    desc.msi_index                  = queue->queue_id;
    desc.pci_dev                    = ctrl->pci_dev;
    desc.pci.msi_attribute.can_mask = false;
    desc.pci.msi_attribute.is_64    = true;
    desc.pci.msi_attribute.is_msix  = true;
    int ret                         = pci_enable_msi(&desc);
    if (ret < 0) {
        printk("Failed to enable MSI/MSI-X\n");
        return -1;
    }

    extern intctl_t apic_controller;
    irq_regist_irq(
        vector + IRQ_BASE_VECTOR,
        nvme_interrupt_handler,
        vector,
        queue,
        &apic_controller,
        "NVMe",
        IRQ_FLAGS_MSIX,
        PCI_MSI
    );
#endif

    g_nvme_platform_ops->log(
        "NVMe: Queue %u bound to interrupt vector %u\n", queue->queue_id, vector
    );

    return 0;
}

// Initialize queue with alignment checks
static int nvme_init_queue(
    nvme_controller_t *ctrl, nvme_queue_t *queue, uint16_t queue_id, uint16_t queue_depth
) {
    queue->ctrl = ctrl;

    queue->lock = SPIN_INIT;

    queue->queue_id    = queue_id;
    queue->queue_depth = queue_depth;
    queue->sq_head     = 0;
    queue->sq_tail     = 0;
    queue->cq_head     = 0;
    queue->cq_phase    = 1;

    size_t sq_size = sizeof(nvme_sqe_t) * queue_depth;
    size_t cq_size = sizeof(nvme_cqe_t) * queue_depth;

    // Allocate submission queue (must be aligned to page size)
    queue->sq = g_nvme_platform_ops->dma_alloc(sq_size, &queue->sq_phys);
    if (!queue->sq) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate SQ\n");
        return -1;
    }

    // Check alignment
    if (queue->sq_phys & 0xFFF) {
        g_nvme_platform_ops->log("NVMe: Warning - SQ not page aligned: 0x%llx\n", queue->sq_phys);
    }

    memset(queue->sq, 0, sq_size);
    g_nvme_platform_ops->log(
        "NVMe: SQ allocated at virt=%p phys=0x%llx size=%d\n", queue->sq, queue->sq_phys, sq_size
    );

    // Allocate completion queue
    queue->cq = g_nvme_platform_ops->dma_alloc(cq_size, &queue->cq_phys);
    if (!queue->cq) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate CQ\n");
        g_nvme_platform_ops->dma_free(queue->sq, sq_size);
        return -1;
    }

    if (queue->cq_phys & 0xFFF) {
        g_nvme_platform_ops->log("NVMe: Warning - CQ not page aligned: 0x%llx\n", queue->cq_phys);
    }

    memset(queue->cq, 0, cq_size);
    g_nvme_platform_ops->log(
        "NVMe: CQ allocated at virt=%p phys=0x%llx size=%d\n", queue->cq, queue->cq_phys, cq_size
    );

    // Calculate doorbell addresses
    uint32_t doorbell_offset = NVME_REG_DBS + (2 * queue_id * ctrl->doorbell_stride);
    queue->sq_doorbell       = (volatile uint32_t *)(ctrl->bar0 + doorbell_offset);
    queue->cq_doorbell =
        (volatile uint32_t *)(ctrl->bar0 + doorbell_offset + ctrl->doorbell_stride);

    g_nvme_platform_ops->log(
        "NVMe: Queue %d doorbells - SQ offset=0x%x CQ offset=0x%x\n",
        queue_id,
        doorbell_offset,
        doorbell_offset + ctrl->doorbell_stride
    );

    int irq = irq_allocate_irqnum();
    nvme_bind_queue_interrupt(ctrl, queue, irq);
    queue->vector = irq;

    return 0;
}

// Submit command with validation
static int nvme_submit_cmd(nvme_queue_t *queue, nvme_sqe_t *cmd) {
    uint16_t tail = queue->sq_tail;

    // Check if queue is full
    uint16_t next_tail = (tail + 1) % queue->queue_depth;
    if (next_tail == queue->sq_head) {
        g_nvme_platform_ops->log(
            "NVMe: Queue %d is full (head=%d tail=%d)\n", queue->queue_id, queue->sq_head, tail
        );
        return -1;
    }

    spin_lock(queue->lock);

    // Copy command to queue
    memcpy(&queue->sq[tail], cmd, sizeof(nvme_sqe_t));

    // Update tail pointer
    queue->sq_tail = next_tail;

    // Memory barrier before ringing doorbell
    g_nvme_platform_ops->wmb();

    // Ring doorbell
    *queue->sq_doorbell = next_tail;

    // Memory barrier after doorbell
    g_nvme_platform_ops->mb();

    spin_unlock(queue->lock);

    return 0;
}

// Process completions from a queue
static int nvme_process_queue_completions(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    int count = 0;

    while (1) {
        // Read memory barrier
        g_nvme_platform_ops->rmb();

        nvme_cqe_t *cqe = &queue->cq[queue->cq_head];
        uint16_t phase  = (cqe->status >> 0) & 1;

        // Check phase bit
        if (phase != queue->cq_phase) {
            break;
        }

        // Extract status
        uint16_t status_code = (cqe->status >> 1) & 0xFF;
        uint16_t status_type = (cqe->status >> 9) & 0x7;
        bool success         = (status_code == 0 && status_type == 0);

        if (!success) {
            g_nvme_platform_ops->log(
                "NVMe: Command failed - CID=%d Status=0x%04x (SC=%d SCT=%d)\n",
                cqe->cid,
                cqe->status,
                status_code,
                status_type
            );
        }

        // Find and invoke callback
        uint16_t cid = cqe->cid;
        if (cid < UINT16_MAX && ctrl->requests[cid]) {
            nvme_request_t *req = ctrl->requests[cid];
            if (req->callback) {
                req->callback(req->ctx, success, cqe->dw0);
            }
            ctrl->requests[cid] = NULL;
            g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        }

        // Update SQ head from completion
        queue->sq_head = cqe->sq_head;

        // Advance CQ head
        queue->cq_head++;
        if (queue->cq_head >= queue->queue_depth) {
            queue->cq_head  = 0;
            queue->cq_phase = !queue->cq_phase;
        }

        count++;
    }

    // Ring completion doorbell if we processed any
    if (count > 0) {
        g_nvme_platform_ops->wmb();
        *queue->cq_doorbell = queue->cq_head;
        g_nvme_platform_ops->mb();
    }

    return count;
}

void nvme_interrupt_handler(uint64_t irq_num, void *data, struct pt_regs *r) {
    nvme_queue_t *queue = (nvme_queue_t *)data;
    while (nvme_process_queue_completions(queue->ctrl, queue))
        ;
}

// Allocate command ID
static uint16_t nvme_alloc_cid(nvme_controller_t *ctrl, nvme_request_t *req) {
    spin_lock(ctrl->cid_alloc_lock);
    for (int i = 0; i < 65536; i++) {
        if (ctrl->requests[i] == NULL) {
            ctrl->requests[i] = req;
            req->cid          = i;
            spin_unlock(ctrl->cid_alloc_lock);
            return i;
        }
    }
    spin_unlock(ctrl->cid_alloc_lock);
    return 0xFFFF;
}

// Execute admin command (synchronous helper)
typedef struct {
    bool done;
    bool success;
    uint32_t result;
} admin_sync_ctx_t;

static void admin_sync_callback(void *ctx, bool success, uint32_t result) {
    admin_sync_ctx_t *sync_ctx = (admin_sync_ctx_t *)ctx;
    sync_ctx->done             = true;
    sync_ctx->success          = success;
    sync_ctx->result           = result;
}

static int nvme_admin_cmd_sync(
    nvme_controller_t *ctrl, nvme_sqe_t *cmd, uint32_t *result, uint32_t timeout_ms
) {
    admin_sync_ctx_t sync_ctx = { 0 };
    nvme_request_t *req       = g_nvme_platform_ops->dma_alloc(sizeof(nvme_request_t), NULL);

    req->callback = admin_sync_callback;
    req->ctx      = &sync_ctx;

    uint16_t cid = nvme_alloc_cid(ctrl, req);
    if (cid == 0xFFFF) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate CID\n");
        return -1;
    }

    // CID 在 bits 31:16，Opcode 已经在 bits 7:0
    cmd->cdw0 = (cmd->cdw0 & 0x0000FFFF) | (cid << 16);

    g_nvme_platform_ops->log(
        "NVMe: Submitting admin command - opcode=%d cid=%d cdw0=0x%08x\n",
        cmd->cdw0 & 0xFF,
        cid,
        cmd->cdw0
    );

    arch_open_interrupt();

    if (nvme_submit_cmd(&ctrl->admin_queue, cmd) != 0) {
        ctrl->requests[cid] = NULL;
        return -1;
    }

    // Poll for completion
    while (!sync_ctx.done) {
        // nvme_process_completions(ctrl);
        arch_pause();
    }
    arch_close_interrupt();

    if (result) {
        *result = sync_ctx.result;
    }

    g_nvme_platform_ops->log("NVMe: Admin command completed - success=%d\n", sync_ctx.success);
    return sync_ctx.success ? 0 : -1;
}

// Identify Controller
static int nvme_identify_controller(nvme_controller_t *ctrl, nvme_identify_ctrl_t *id_ctrl) {
    uint64_t buffer_phys;
    void *buffer = g_nvme_platform_ops->dma_alloc(PAGE_SIZE, &buffer_phys);
    if (!buffer) {
        return -1;
    }
    memset(buffer, 0, PAGE_SIZE);

    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_IDENTIFY;
    cmd.prp1       = buffer_phys;
    cmd.cdw10      = 1; // CNS = 01h (Identify Controller)

    int ret = nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);

    if (ret == 0) {
        memcpy(id_ctrl, buffer, sizeof(nvme_identify_ctrl_t));
    }

    g_nvme_platform_ops->dma_free(buffer, PAGE_SIZE);
    return ret;
}

static int
nvme_identify_namespace(nvme_controller_t *ctrl, uint32_t nsid, nvme_identify_ns_t *id_ns) {
    uint64_t buffer_phys;
    void *buffer = g_nvme_platform_ops->dma_alloc(PAGE_SIZE, &buffer_phys);
    if (!buffer) {
        return -1;
    }
    memset(buffer, 0, PAGE_SIZE);

    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_IDENTIFY;
    cmd.nsid       = nsid;
    cmd.prp1       = buffer_phys;
    cmd.cdw10      = 0; // CNS = 00h (Identify Namespace)

    int ret = nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);

    if (ret == 0) {
        memcpy(id_ns, buffer, sizeof(nvme_identify_ns_t));
    }

    g_nvme_platform_ops->dma_free(buffer, PAGE_SIZE);
    return ret;
}

// create I/O Completion Queue
static int nvme_create_io_cq(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_CREATE_CQ;
    cmd.prp1       = queue->cq_phys;
    cmd.cdw10      = ((queue->queue_depth - 1) << 16) | queue->queue_id;

    uint16_t vector = queue->vector;
    uint32_t cdw11  = 0;

    cdw11 |= (1 << 0);                          // PC (Physically Contiguous) = 1
    cdw11 |= (1 << 1);                          // IEN (Interrupts Enabled) = 1
    cdw11 |= ((uint32_t)queue->queue_id << 16); // IV (Interrupt Vector)

    cmd.cdw11 = cdw11;

    g_nvme_platform_ops->log(
        "NVMe: Creating I/O CQ %d (depth=%d, phys=0x%llx)\n",
        queue->queue_id,
        queue->queue_depth,
        queue->cq_phys
    );

    return nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
}

//  Create I/O Submission Queue
static int nvme_create_io_sq(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_CREATE_SQ;
    cmd.prp1       = queue->sq_phys;
    cmd.cdw10      = ((queue->queue_depth - 1) << 16) | queue->queue_id;
    cmd.cdw11      = (queue->queue_id << 16) | 0x1; // CQID, PC=1

    g_nvme_platform_ops->log(
        "NVMe: Creating I/O SQ %d (depth=%d, phys=0x%llx)\n",
        queue->queue_id,
        queue->queue_depth,
        queue->sq_phys
    );

    return nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
}

// Check PCI configuration
static int nvme_check_pci_config(pci_device_t *device) {
    g_nvme_platform_ops->log("NVMe: Checking PCI configuration...\n");

    // Check BAR0
    if (!device->bars[0].address || device->bars[0].size < 0x2000) {
        g_nvme_platform_ops->log(
            "NVMe: Invalid BAR0 (addr=0x%llx size=0x%llx)\n",
            device->bars[0].address,
            device->bars[0].size
        );
        return -1;
    }

    if (!device->bars[0].mmio) {
        g_nvme_platform_ops->log("NVMe: BAR0 is not MMIO\n");
        return -1;
    }

    g_nvme_platform_ops->log(
        "NVMe: BAR0 at 0x%llx, size 0x%llx\n", device->bars[0].address, device->bars[0].size
    );

    // TODO: Enable PCI bus mastering and memory space if your platform requires
    // it This is platform-specific

    return 0;
}

// 计算传输需要的页面数量
static inline uint32_t nvme_calc_num_pages(uint64_t addr, uint32_t size) {
    uint64_t end = addr + size - 1;
    return (end >> 12) - (addr >> 12) + 1;
}

// 获取页内偏移
static inline uint32_t nvme_page_offset(uint64_t addr) {
    return addr & NVME_PAGE_MASK;
}

// 初始化 PRP List 池
static int nvme_init_prp_pool(nvme_controller_t *ctrl, uint32_t num_lists) {
    size_t pool_size = sizeof(nvme_prp_list_t) * num_lists;

    ctrl->prp_list_pool =
        (nvme_prp_list_t *)g_nvme_platform_ops->dma_alloc(pool_size, &ctrl->prp_list_pool_phys);

    if (!ctrl->prp_list_pool) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate PRP list pool\n");
        return -1;
    }

    memset(ctrl->prp_list_pool, 0, pool_size);
    ctrl->prp_list_pool_size = num_lists;
    ctrl->prp_list_next_free = 0;

    g_nvme_platform_ops->log(
        "NVMe: PRP pool initialized: %u lists, phys=0x%llx\n", num_lists, ctrl->prp_list_pool_phys
    );

    return 0;
}

// 分配 PRP List（简单的循环分配）
static nvme_prp_list_t *nvme_alloc_prp_list(nvme_controller_t *ctrl, uint64_t *phys_addr) {
    if (ctrl->prp_list_next_free >= ctrl->prp_list_pool_size) {
        ctrl->prp_list_next_free = 0; // 循环使用
    }

    uint32_t idx          = ctrl->prp_list_next_free++;
    nvme_prp_list_t *list = &ctrl->prp_list_pool[idx];

    if (phys_addr) {
        *phys_addr = ctrl->prp_list_pool_phys + (idx * sizeof(nvme_prp_list_t));
    }

    memset(list, 0, sizeof(nvme_prp_list_t));
    return list;
}

// 设置 PRP 条目（核心优化函数）
static int
nvme_setup_prp(nvme_controller_t *ctrl, nvme_sqe_t *cmd, uint64_t phys_addr, uint32_t size) {
    if (size == 0) {
        g_nvme_platform_ops->log("NVMe: Invalid PRP size 0\n");
        return -1;
    }

    // 检查是否超过最大传输大小
    if (ctrl->max_transfer_size > 0 && size > ctrl->max_transfer_size) {
        g_nvme_platform_ops->log(
            "NVMe: Transfer size %u exceeds MDTS %u\n", size, ctrl->max_transfer_size
        );
        return -1;
    }

    uint32_t page_offset = nvme_page_offset(phys_addr);
    uint32_t num_pages   = nvme_calc_num_pages(phys_addr, size);

    // PRP1 总是指向第一个地址
    cmd->prp1 = phys_addr;
    cmd->prp2 = 0;

    // 情况 1: 单页传输（最常见的小 I/O）
    if (num_pages == 1) {
        return 0;
    }

    // 计算第一页剩余空间
    uint32_t first_page_size = NVME_PAGE_SIZE - page_offset;
    uint64_t next_addr       = phys_addr + first_page_size;
    uint32_t remaining       = size - first_page_size;

    // 情况 2: 双页传输（PRP1 指向第一页，PRP2 指向第二页）
    if (num_pages == 2) {
        cmd->prp2 = next_addr;
        return 0;
    }

    // 情况 3: 多页传输（需要 PRP List）

    uint64_t prp_list_phys;
    nvme_prp_list_t *prp_list = nvme_alloc_prp_list(ctrl, &prp_list_phys);
    if (!prp_list) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate PRP list\n");
        return -1;
    }

    // PRP2 指向 PRP List
    cmd->prp2 = prp_list_phys;

    // 填充 PRP List（从第二页开始）
    uint32_t prp_idx      = 0;
    uint64_t current_addr = next_addr;

    // 跳过第一页（已在 PRP1），从第二页开始
    for (uint32_t page = 1; page < num_pages; page++) {
        if (prp_idx >= NVME_MAX_PRP_LIST_ENTRIES) {
            g_nvme_platform_ops->log(
                "NVMe: PRP list overflow! pages=%u max_entries=%u\n",
                num_pages,
                NVME_MAX_PRP_LIST_ENTRIES
            );
            return -1;
        }

        prp_list->prp[prp_idx] = current_addr;

        current_addr += NVME_PAGE_SIZE;
        prp_idx++;
    }

    // 确保 PRP List 写入内存
    g_nvme_platform_ops->wmb();

    return 0;
}

// 优化的异步读取（支持 PRP）
int nvme_read_async(
    nvme_controller_t *ctrl,
    uint32_t nsid,
    uint64_t lba,
    uint32_t block_count,
    void *buffer,
    uint64_t buffer_phys,
    nvme_io_callback_t callback,
    void *ctx
) {
    if (!ctrl->initialized || nsid == 0 || nsid > ctrl->num_namespaces) {
        g_nvme_platform_ops->log("NVMe: Invalid namespace %u\n", nsid);
        return -1;
    }

    if (!ctrl->namespaces[nsid - 1].valid) {
        g_nvme_platform_ops->log("NVMe: Namespace %u not valid\n", nsid);
        return -1;
    }

    uint32_t block_size    = ctrl->namespaces[nsid - 1].block_size;
    uint32_t transfer_size = block_count * block_size;

    // 检查传输大小
    if (ctrl->max_transfer_size > 0 && transfer_size > ctrl->max_transfer_size) {
        g_nvme_platform_ops->log(
            "NVMe: Transfer too large: %u > %u bytes\n", transfer_size, ctrl->max_transfer_size
        );
        return -1;
    }

    // 分配请求
    nvme_request_t *req =
        (nvme_request_t *)g_nvme_platform_ops->dma_alloc(sizeof(nvme_request_t), NULL);
    if (!req) {
        return -1;
    }

    req->callback = callback;
    req->ctx      = ctx;

    uint16_t cid = nvme_alloc_cid(ctrl, req);
    if (cid == 0xFFFF) {
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    // 构造读取命令
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_CMD_READ | (cid << 16);
    cmd.nsid       = nsid;
    cmd.cdw10      = lba & 0xFFFFFFFF;           // LBA lower 32 bits
    cmd.cdw11      = lba >> 32;                  // LBA upper 32 bits
    cmd.cdw12      = (block_count - 1) & 0xFFFF; // 0's based value

    // 设置 PRP
    if (nvme_setup_prp(ctrl, &cmd, buffer_phys, transfer_size) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to setup PRP\n");
        ctrl->requests[cid] = NULL;
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    // 提交命令
    if (nvme_submit_cmd(&ctrl->io_queues[arch_current_cpu()->id], &cmd) != 0) {
        ctrl->requests[cid] = NULL;
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    return 0;
}

// 优化的异步写入（支持 PRP）
int nvme_write_async(
    nvme_controller_t *ctrl,
    uint32_t nsid,
    uint64_t lba,
    uint32_t block_count,
    const void *buffer,
    uint64_t buffer_phys,
    nvme_io_callback_t callback,
    void *ctx
) {
    if (!ctrl->initialized || nsid == 0 || nsid > ctrl->num_namespaces) {
        g_nvme_platform_ops->log("NVMe: Invalid namespace %u\n", nsid);
        return -1;
    }

    if (!ctrl->namespaces[nsid - 1].valid) {
        g_nvme_platform_ops->log("NVMe: Namespace %u not valid\n", nsid);
        return -1;
    }

    uint32_t block_size    = ctrl->namespaces[nsid - 1].block_size;
    uint32_t transfer_size = block_count * block_size;

    // 检查传输大小
    if (ctrl->max_transfer_size > 0 && transfer_size > ctrl->max_transfer_size) {
        g_nvme_platform_ops->log(
            "NVMe: Transfer too large: %u > %u bytes\n", transfer_size, ctrl->max_transfer_size
        );
        return -1;
    }

    // 分配请求
    nvme_request_t *req =
        (nvme_request_t *)g_nvme_platform_ops->dma_alloc(sizeof(nvme_request_t), NULL);
    if (!req) {
        printk("NVMe: Failed allocate request!\n");
        return -1;
    }

    req->callback = callback;
    req->ctx      = ctx;

    uint16_t cid = nvme_alloc_cid(ctrl, req);
    if (cid == 0xFFFF) {
        printk("NVMe: Failed allocate command id!\n");
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    // 构造写入命令
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_CMD_WRITE | (cid << 16);
    cmd.nsid       = nsid;
    cmd.cdw10      = lba & 0xFFFFFFFF;
    cmd.cdw11      = lba >> 32;
    cmd.cdw12      = (block_count - 1) & 0xFFFF;

    // 设置 PRP
    if (nvme_setup_prp(ctrl, &cmd, buffer_phys, transfer_size) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to setup PRP\n");
        ctrl->requests[cid] = NULL;
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    // 确保数据写入内存
    g_nvme_platform_ops->wmb();

    // 提交命令
    if (nvme_submit_cmd(&ctrl->io_queues[arch_current_cpu()->id], &cmd) != 0) {
        ctrl->requests[cid] = NULL;
        g_nvme_platform_ops->dma_free(req, sizeof(nvme_request_t));
        return -1;
    }

    return 0;
}

typedef struct nvme_callback_ctx {
    bool completed;
    bool success;
} nvme_callback_ctx_t;

void nvme_io_callback(void *ctx, bool success, uint32_t result) {
    nvme_callback_ctx_t *cb_ctx = ctx;
    cb_ctx->success             = success;
    cb_ctx->completed           = true;
}

typedef struct nvme_ns {
    nvme_controller_t *ctrl;
    nvme_namespace_t *ns;
} nvme_ns_t;

size_t nvme_read(void *data, uint8_t *buffer, size_t size, size_t lba) {
    nvme_ns_t *ns = data;

    nvme_queue_t *queue = &ns->ctrl->io_queues[arch_current_cpu()->id];

    nvme_callback_ctx_t *cb_ctx = malloc(sizeof(nvme_callback_ctx_t));
    cb_ctx->completed           = false;
    cb_ctx->success             = false;
    bool en                     = arch_check_interrupt();
    arch_open_interrupt();
    int r = nvme_read_async(
        ns->ctrl,
        ns->ns->nsid,
        lba,
        size,
        buffer,
        arch_virt_to_phys((uint64_t)buffer),
        nvme_io_callback,
        cb_ctx
    );
    if (r < 0) {
        printk("NVMe: submit command failure!\n");
        return 0;
    }
    bool timeout        = true;
    uint64_t timeout_ns = nano_time() + 500ULL * 1000000ULL;
    while (nano_time() < timeout_ns) {
        if (cb_ctx->completed) {
            timeout = false;
            break;
        }
        arch_wait_for_interrupt();
    }
    if (!en)
        arch_close_interrupt();
    if (timeout) {
        while (nvme_process_queue_completions(ns->ctrl, queue))
            ;
        if (!cb_ctx->completed) {
            printk("NVMe: timeout!!!\n");
            free(cb_ctx);
            return 0;
        }
    }
    uint64_t ret = 0;
    if (cb_ctx->success) {
        ret = size;
    } else {
        printk("NVMe: Command not successful!\n");
    }
    free(cb_ctx);
    return ret;
}

size_t nvme_write(void *data, uint8_t *buffer, size_t size, size_t lba) {
    nvme_ns_t *ns = data;

    nvme_queue_t *queue = &ns->ctrl->io_queues[arch_current_cpu()->id];

    nvme_callback_ctx_t *cb_ctx = malloc(sizeof(nvme_callback_ctx_t));
    cb_ctx->completed           = false;
    cb_ctx->success             = false;
    arch_open_interrupt();
    int r = nvme_write_async(
        ns->ctrl,
        ns->ns->nsid,
        lba,
        size,
        buffer,
        arch_virt_to_phys((uint64_t)buffer),
        nvme_io_callback,
        cb_ctx
    );
    if (r < 0) {
        printk("NVMe: submit command failure!\n");
        return 0;
    }
    bool timeout        = true;
    uint64_t timeout_ns = nano_time() + 500ULL * 1000000ULL;
    while (nano_time() < timeout_ns) {
        if (cb_ctx->completed) {
            timeout = false;
            break;
        }
        arch_wait_for_interrupt();
    }
    arch_close_interrupt();
    if (timeout) {
        while (nvme_process_queue_completions(ns->ctrl, queue))
            ;
        if (!cb_ctx->completed) {
            printk("NVMe: timeout!!!\n");
            free(cb_ctx);
            return 0;
        }
    }
    uint64_t ret = 0;
    if (cb_ctx->success) {
        ret = size;
    } else {
        printk("NVMe: Command not successful!\n");
    }
    free(cb_ctx);
    return ret;
}

// Main probe function
void nvme_probe(pci_device_t *device) {
    if (!g_nvme_platform_ops) {
        return;
    }

    g_nvme_platform_ops->log(
        "NVMe: Probing device %04x:%04x\n", device->vendor_id, device->device_id
    );

    // Allocate controller structure
    nvme_controller_t *ctrl =
        (nvme_controller_t *)g_nvme_platform_ops->dma_alloc(sizeof(nvme_controller_t), NULL);
    if (!ctrl) {
        return;
    }
    memset(ctrl, 0, sizeof(nvme_controller_t));

    ctrl->cid_alloc_lock = SPIN_INIT;

    ctrl->pci_dev = device;
    ctrl->bar0    = phys_to_virt(device->bars[0].address);
    page_map_range(
        get_kernel_pagedir(),
        (uint64_t)ctrl->bar0,
        device->bars[0].address,
        device->bars[0].size,
        KERNEL_PTE_FLAGS
    );

    if (!ctrl->bar0) {
        g_nvme_platform_ops->log("NVMe: BAR0 not mapped\n");
        goto error;
    }

    // Read capabilities
    uint64_t cap          = NVME_READ64(ctrl, NVME_REG_CAP);
    ctrl->doorbell_stride = 4 << ((cap >> 32) & 0xF);
    uint32_t mpsmin       = (cap >> 48) & 0xF;
    uint32_t mpsmax       = (cap >> 52) & 0xF;

    g_nvme_platform_ops->log("NVMe: CAP=%016llx, doorbell_stride=%d\n", cap, ctrl->doorbell_stride);

    // Disable controller
    if (nvme_disable_controller(ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to disable controller\n");
        goto error;
    }

    // Initialize admin queue
    if (nvme_init_queue(ctrl, &ctrl->admin_queue, 0, NVME_ADMIN_QUEUE_SIZE) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to initialize admin queue\n");
        goto error;
    }

    // Configure admin queues
    NVME_WRITE32(
        ctrl, NVME_REG_AQA, ((NVME_ADMIN_QUEUE_SIZE - 1) << 16) | (NVME_ADMIN_QUEUE_SIZE - 1)
    );
    NVME_WRITE64(ctrl, NVME_REG_ASQ, ctrl->admin_queue.sq_phys);
    NVME_WRITE64(ctrl, NVME_REG_ACQ, ctrl->admin_queue.cq_phys);

    // Enable controller
    if (nvme_enable_controller(ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to enable controller\n");
        goto error;
    }

    g_nvme_platform_ops->log("NVMe: Controller ready\n");

    // Identify controller
    nvme_identify_ctrl_t id_ctrl;
    if (nvme_identify_controller(ctrl, &id_ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to identify controller\n");
        goto error;
    }

    ctrl->num_namespaces = id_ctrl.nn;

    if (id_ctrl.mdts) {
        ctrl->max_transfer_size = ((1ULL << id_ctrl.mdts) * PAGE_SIZE);
    } else {
        ctrl->max_transfer_size = -1;
    }
    g_nvme_platform_ops->log(
        "NVMe: Model=%.40s, Namespaces=%d\n", id_ctrl.mn, ctrl->num_namespaces
    );

    ctrl->num_io_queues = MIN(8, get_cpu_count());

    uint32_t num_prp_lists = NVME_IO_QUEUE_SIZE * ctrl->num_io_queues;
    if (nvme_init_prp_pool(ctrl, num_prp_lists) != 0) {
        goto error;
    }

    ctrl->page_size = NVME_PAGE_SIZE;

    // Create I/O queue pair
    for (uint32_t qid = 0; qid < ctrl->num_io_queues; qid++) {
        if (nvme_init_queue(ctrl, &ctrl->io_queues[qid], 1 + qid, NVME_IO_QUEUE_SIZE) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to create I/O queue\n");
            goto error;
        }

        if (nvme_create_io_cq(ctrl, &ctrl->io_queues[qid]) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to create I/O CQ\n");
            goto error;
        }

        if (nvme_create_io_sq(ctrl, &ctrl->io_queues[qid]) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to create I/O SQ\n");
            goto error;
        }
    }

    g_nvme_platform_ops->log("NVMe: I/O queues created\n");

    ctrl->initialized = true;

    // Identify namespaces
    // for (uint32_t i = 1; i <= ctrl->num_namespaces && i <= 256; i++) {
    for (uint32_t i = 1; i <= 1 && i <= 256; i++) {
        nvme_identify_ns_t id_ns;
        if (nvme_identify_namespace(ctrl, i, &id_ns) == 0 && id_ns.nsze > 0) {
            ctrl->namespaces[i - 1].nsid        = i;
            ctrl->namespaces[i - 1].block_count = id_ns.nsze;

            uint8_t lba_format = id_ns.flbas & 0xF;
            if (lba_format < 16) {
                ctrl->namespaces[i - 1].block_size = 1 << id_ns.lbaf[lba_format].lbads;
            }
            ctrl->namespaces[i - 1].valid = true;

            g_nvme_platform_ops->log(
                "NVMe: NS%d: %lld blocks x %d bytes\n",
                i,
                id_ns.nsze,
                ctrl->namespaces[i - 1].block_size
            );

            nvme_ns_t *ns = malloc(sizeof(nvme_ns_t));
            ns->ctrl      = ctrl;
            ns->ns        = &ctrl->namespaces[i - 1];

            blk_device_t *device_b = malloc(sizeof(blk_device_t));
            sprintf(device_b->name, "nvme%d", i);
            device_b->handle     = ns;
            device_b->ops.write  = nvme_write;
            device_b->ops.read   = nvme_read;
            device_b->block_size = ns->ns->block_size;
            device_b->size       = ns->ns->block_count * ns->ns->block_size;
            device_b->max_size   = ctrl->max_transfer_size;
            register_device(device_b);
        }
    }

    device->desc = ctrl;

    g_nvme_platform_ops->log("NVMe: Initialization complete\n");
    return;

error:
    // Cleanup on error
    if (ctrl) {
        nvme_dump_status(ctrl);
        g_nvme_platform_ops->dma_free(ctrl, sizeof(nvme_controller_t));
    }
}

void nvme_setup() {
    nvme_set_platform_ops(&nvme_platform_ops);
    pci_find_class(0x00010802, nvme_probe);
}
