#include "nvme.h"
#include "driver_subsystem.h"
#include "cp_kernel.h"
#include "errno.h"
#include "mem_subsystem.h"

static size_t nvme_controller_index = 0;

static errno_t dummy() {
    return 0;
}

static void *cpkrnl_dma_alloc(size_t size, uint64_t *phys_addr) {
    size_t bytes  = PADDING_UP(MAX(size, (size_t)1), PAGE_SIZE);
    size_t pages  = bytes / PAGE_SIZE;
    uint64_t phys = alloc_frames(pages);
    void *virt    = driver_phys_to_virt(phys);

    if (virt == NULL) {
        free_frames(phys, pages);
        return NULL;
    }

    page_map_range(get_kernel_pagedir(), (uint64_t)virt, phys, bytes, get_kernel_pte_flags());
    memset(virt, 0, bytes);

    if (phys_addr) {
        *phys_addr = phys;
    }

    return virt;
}

static void cpkrnl_dma_free(void *virt, size_t size) {
    if (virt == NULL) {
        return;
    }

    size_t bytes = PADDING_UP(MAX(size, (size_t)1), PAGE_SIZE);
    unmap_page_range(get_kernel_pagedir(), (uint64_t)virt, bytes);
}

static void cpkrnl_memory_barrier(void) {
    __sync_synchronize();
}

static void cpkrnl_read_barrier(void) {
    __sync_synchronize();
}

static void cpkrnl_write_barrier(void) {
    __sync_synchronize();
}

static void cpkrnl_udelay(uint32_t us) {
    uint64_t end = nano_time() + (uint64_t)us * 1000;
    while (nano_time() < end) {
        arch_pause();
    }
}

static uint64_t cpkrnl_get_time_ms(void) {
    return nano_time() / 1000000;
}

static void *cpkrnl_mutex_create(void) {
    return NULL;
}

static void cpkrnl_mutex_lock(void *mutex) {
    (void)mutex;
}

static void cpkrnl_mutex_unlock(void *mutex) {
    (void)mutex;
}

static void cpkrnl_mutex_destroy(void *mutex) {
    (void)mutex;
}

static int cpkrnl_printk(const char *fmt, ...) {
    char buf[2048];
    va_list args;

    memset(buf, 0, sizeof(buf));

    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    printk(buf);
    return n;
}

static nvme_platform_ops_t cpkrnl_nvme_platform_ops = {
    .dma_alloc     = cpkrnl_dma_alloc,
    .dma_free      = cpkrnl_dma_free,
    .mb            = cpkrnl_memory_barrier,
    .rmb           = cpkrnl_read_barrier,
    .wmb           = cpkrnl_write_barrier,
    .udelay        = cpkrnl_udelay,
    .get_time_ms   = cpkrnl_get_time_ms,
    .mutex_create  = cpkrnl_mutex_create,
    .mutex_lock    = cpkrnl_mutex_lock,
    .mutex_unlock  = cpkrnl_mutex_unlock,
    .mutex_destroy = cpkrnl_mutex_destroy,
    .log           = cpkrnl_printk,
};

nvme_platform_ops_t *g_nvme_platform_ops = NULL;

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

static void nvme_set_platform_ops(nvme_platform_ops_t *ops) {
    g_nvme_platform_ops = ops;
}

static void nvme_dump_status(nvme_controller_t *ctrl) {
    if (ctrl == NULL || ctrl->bar0 == NULL) {
        return;
    }

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

static int nvme_wait_ready(nvme_controller_t *ctrl, bool ready, uint32_t timeout_ms) {
    uint64_t start     = g_nvme_platform_ops->get_time_ms();
    uint32_t last_csts = 0;

    while (true) {
        uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);

        if (csts & NVME_CSTS_CFS) {
            g_nvme_platform_ops->log("NVMe: Controller Fatal Status detected\n");
            nvme_dump_status(ctrl);
            return -1;
        }

        if (((csts & NVME_CSTS_RDY) != 0) == ready) {
            return 0;
        }

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

static int nvme_reset_controller(nvme_controller_t *ctrl) {
    uint32_t cc   = NVME_READ32(ctrl, NVME_REG_CC);
    uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);

    g_nvme_platform_ops->log("NVMe: Resetting controller\n");
    g_nvme_platform_ops->log("NVMe: Initial state - CC=0x%08x CSTS=0x%08x\n", cc, csts);

    if (cc & NVME_CC_ENABLE) {
        cc &= ~NVME_CC_ENABLE;
        NVME_WRITE32(ctrl, NVME_REG_CC, cc);

        if (nvme_wait_ready(ctrl, false, 5000) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to disable controller\n");
            return -1;
        }
    } else if (csts & NVME_CSTS_RDY) {
        if (nvme_wait_ready(ctrl, false, 5000) != 0) {
            return -1;
        }
    }

    return 0;
}

static int nvme_disable_controller(nvme_controller_t *ctrl) {
    return nvme_reset_controller(ctrl);
}

static int nvme_enable_controller(nvme_controller_t *ctrl) {
    uint32_t csts = NVME_READ32(ctrl, NVME_REG_CSTS);

    if (csts & NVME_CSTS_RDY) {
        g_nvme_platform_ops->log("NVMe: Controller still ready, cannot enable\n");
        return -1;
    }

    uint32_t cc = 0;
    cc |= NVME_CC_ENABLE;
    cc |= NVME_CC_CSS_NVM;
    cc |= NVME_CC_AMS_RR;
    cc |= NVME_CC_SHN_NONE;
    cc |= NVME_CC_IOSQES;
    cc |= NVME_CC_IOCQES;

    NVME_WRITE32(ctrl, NVME_REG_CC, cc);

    if (NVME_READ32(ctrl, NVME_REG_CC) != cc) {
        g_nvme_platform_ops->log("NVMe: Failed to program CC register\n");
        return -1;
    }

    return nvme_wait_ready(ctrl, true, 10000);
}

static int nvme_init_queue(
    nvme_controller_t *ctrl, nvme_queue_t *queue, uint16_t queue_id, uint16_t queue_depth
) {
    size_t sq_size           = sizeof(nvme_sqe_t) * queue_depth;
    size_t cq_size           = sizeof(nvme_cqe_t) * queue_depth;
    uint32_t doorbell_offset = NVME_REG_DBS + (2 * queue_id * ctrl->doorbell_stride);

    memset(queue, 0, sizeof(*queue));
    queue->ctrl        = ctrl;
    queue->lock        = SPIN_INIT;
    queue->queue_id    = queue_id;
    queue->queue_depth = queue_depth;
    queue->cq_phase    = 1;

    queue->sq = g_nvme_platform_ops->dma_alloc(sq_size, &queue->sq_phys);
    if (queue->sq == NULL) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate SQ\n");
        return -1;
    }

    queue->cq = g_nvme_platform_ops->dma_alloc(cq_size, &queue->cq_phys);
    if (queue->cq == NULL) {
        g_nvme_platform_ops->log("NVMe: Failed to allocate CQ\n");
        g_nvme_platform_ops->dma_free(queue->sq, sq_size);
        memset(queue, 0, sizeof(*queue));
        return -1;
    }

    memset(queue->sq, 0, sq_size);
    memset(queue->cq, 0, cq_size);

    queue->sq_doorbell = (volatile uint32_t *)(ctrl->bar0 + doorbell_offset);
    queue->cq_doorbell =
        (volatile uint32_t *)(ctrl->bar0 + doorbell_offset + ctrl->doorbell_stride);

    g_nvme_platform_ops->log(
        "NVMe: Queue %u SQ=%p CQ=%p SQDB=%p CQDB=%p\n",
        queue_id,
        queue->sq,
        queue->cq,
        queue->sq_doorbell,
        queue->cq_doorbell
    );

    return 0;
}

static void nvme_free_queue(nvme_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    if (queue->sq) {
        g_nvme_platform_ops->dma_free(queue->sq, sizeof(nvme_sqe_t) * queue->queue_depth);
    }
    if (queue->cq) {
        g_nvme_platform_ops->dma_free(queue->cq, sizeof(nvme_cqe_t) * queue->queue_depth);
    }

    memset(queue, 0, sizeof(*queue));
}

static int nvme_submit_cmd(nvme_queue_t *queue, const nvme_sqe_t *cmd) {
    spin_lock(queue->lock);

    uint16_t tail      = queue->sq_tail;
    uint16_t next_tail = (tail + 1) % queue->queue_depth;

    if (next_tail == queue->sq_head) {
        spin_unlock(queue->lock);
        return -1;
    }

    memcpy(&queue->sq[tail], cmd, sizeof(*cmd));
    queue->sq_tail = next_tail;

    g_nvme_platform_ops->wmb();
    *queue->sq_doorbell = next_tail;
    g_nvme_platform_ops->mb();

    spin_unlock(queue->lock);
    return 0;
}

static inline void nvme_reset_request(nvme_request_t *req) {
    req->callback = NULL;
    req->ctx      = NULL;
    req->next     = NULL;
    if (req->prp_list) {
        memset(req->prp_list, 0, sizeof(*req->prp_list));
    }
}

static inline void
nvme_complete_request(nvme_controller_t *ctrl, uint16_t cid, bool success, uint32_t result) {
    if (cid >= NVME_MAX_REQUESTS) {
        return;
    }

    nvme_request_t *req = __atomic_exchange_n(&ctrl->requests[cid], NULL, __ATOMIC_ACQ_REL);
    if (req == NULL) {
        return;
    }

    nvme_io_callback_t callback = req->callback;
    void *ctx                   = req->ctx;

    nvme_reset_request(req);

    if (callback) {
        callback(ctx, success, result);
    }
}

static void nvme_log_cqe_error(nvme_queue_t *queue, const nvme_cqe_t *cqe) {
    uint16_t status_code = (cqe->status >> 1) & 0xFF;
    uint16_t status_type = (cqe->status >> 9) & 0x7;
    uint16_t crd         = (cqe->status >> 12) & 0x3;
    bool more            = (cqe->status & (1u << 14)) != 0;
    bool dnr             = (cqe->status & (1u << 15)) != 0;

    g_nvme_platform_ops->log(
        "NVMe: CQE error qid=%u cid=%u sq_head=%u sct=%u sc=%u crd=%u more=%d dnr=%d dw0=0x%08x\n",
        queue->queue_id,
        cqe->cid,
        cqe->sq_head,
        status_type,
        status_code,
        crd,
        more,
        dnr,
        cqe->dw0
    );
}

static int nvme_process_queue_completions(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    int count = 0;

    while (true) {
        uint16_t cid;
        uint32_t result;
        bool success;

        spin_lock(queue->lock);
        g_nvme_platform_ops->rmb();

        nvme_cqe_t *cqe = &queue->cq[queue->cq_head];
        uint16_t phase  = cqe->status & 1;

        if (phase != queue->cq_phase) {
            spin_unlock(queue->lock);
            break;
        }

        uint16_t status_code = (cqe->status >> 1) & 0xFF;
        uint16_t status_type = (cqe->status >> 9) & 0x7;

        success = (status_code == 0 && status_type == 0);
        cid     = cqe->cid;
        result  = cqe->dw0;

        if (!success) {
            nvme_log_cqe_error(queue, cqe);
        }

        queue->sq_head = cqe->sq_head;
        queue->cq_head++;
        if (queue->cq_head >= queue->queue_depth) {
            queue->cq_head  = 0;
            queue->cq_phase = !queue->cq_phase;
        }

        g_nvme_platform_ops->wmb();
        *queue->cq_doorbell = queue->cq_head;
        g_nvme_platform_ops->mb();

        spin_unlock(queue->lock);

        nvme_complete_request(ctrl, cid, success, result);
        count++;
    }

    return count;
}

static uint16_t nvme_alloc_cid(nvme_controller_t *ctrl, nvme_io_callback_t callback, void *ctx) {
    spin_lock(ctrl->cid_alloc_lock);

    for (uint16_t i = 0; i < NVME_MAX_REQUESTS; i++) {
        uint16_t cid = (ctrl->cid_alloc_pos + i) % NVME_MAX_REQUESTS;

        if (__atomic_load_n(&ctrl->requests[cid], __ATOMIC_ACQUIRE) != NULL) {
            continue;
        }

        nvme_request_t *slot = &ctrl->request_slots[cid];
        nvme_reset_request(slot);
        slot->cid      = cid;
        slot->callback = callback;
        slot->ctx      = ctx;

        __atomic_store_n(&ctrl->requests[cid], slot, __ATOMIC_RELEASE);
        ctrl->cid_alloc_pos = (cid + 1) % NVME_MAX_REQUESTS;

        spin_unlock(ctrl->cid_alloc_lock);
        return cid;
    }

    spin_unlock(ctrl->cid_alloc_lock);
    return UINT16_MAX;
}

static inline void nvme_release_cid(nvme_controller_t *ctrl, uint16_t cid) {
    if (cid >= NVME_MAX_REQUESTS) {
        return;
    }

    nvme_request_t *req = __atomic_exchange_n(&ctrl->requests[cid], NULL, __ATOMIC_ACQ_REL);
    if (req) {
        nvme_reset_request(req);
    }
}

static void admin_sync_callback(void *ctx, bool success, uint32_t result) {
    admin_sync_ctx_t *sync_ctx = ctx;
    sync_ctx->success          = success;
    sync_ctx->result           = result;
    __atomic_store_n(&sync_ctx->done, true, __ATOMIC_RELEASE);
}

static int nvme_admin_cmd_sync(
    nvme_controller_t *ctrl, nvme_sqe_t *cmd, uint32_t *result, uint32_t timeout_ms
) {
    admin_sync_ctx_t sync_ctx = { 0 };
    if (timeout_ms == 0) {
        timeout_ms = 5000;
    }

    const uint16_t cid = nvme_alloc_cid(ctrl, admin_sync_callback, &sync_ctx);
    if (cid == UINT16_MAX) {
        return -1;
    }

    cmd->cdw0 = (cmd->cdw0 & 0x0000FFFF) | ((uint32_t)cid << 16);

    if (nvme_submit_cmd(&ctrl->admin_queue, cmd) != 0) {
        nvme_release_cid(ctrl, cid);
        return -1;
    }

    uint64_t start = g_nvme_platform_ops->get_time_ms();
    while (!__atomic_load_n(&sync_ctx.done, __ATOMIC_ACQUIRE)) {
        if (!nvme_process_queue_completions(ctrl, &ctrl->admin_queue)) {
            arch_pause();
        }

        if (g_nvme_platform_ops->get_time_ms() - start > timeout_ms) {
            g_nvme_platform_ops->log(
                "NVMe: admin command opcode=0x%02x cid=%u timed out after %u ms\n",
                cmd->cdw0 & 0xFF,
                cid,
                timeout_ms
            );
            nvme_dump_status(ctrl);
            return -1;
        }
    }

    if (result) {
        *result = sync_ctx.result;
    }

    bool success = sync_ctx.success;
    return success ? 0 : -1;
}

static int nvme_identify_controller(nvme_controller_t *ctrl, nvme_identify_ctrl_t *id_ctrl) {
    uint64_t buffer_phys;
    void *buffer = g_nvme_platform_ops->dma_alloc(NVME_PAGE_SIZE, &buffer_phys);

    if (buffer == NULL) {
        return -1;
    }

    memset(buffer, 0, NVME_PAGE_SIZE);

    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_IDENTIFY;
    cmd.prp1       = buffer_phys;
    cmd.cdw10      = 1;

    int ret = nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
    if (ret == 0) {
        memcpy(id_ctrl, buffer, sizeof(*id_ctrl));
    }

    g_nvme_platform_ops->dma_free(buffer, NVME_PAGE_SIZE);
    return ret;
}

static int
nvme_identify_namespace(nvme_controller_t *ctrl, uint32_t nsid, nvme_identify_ns_t *id_ns) {
    uint64_t buffer_phys;
    void *buffer = g_nvme_platform_ops->dma_alloc(NVME_PAGE_SIZE, &buffer_phys);

    if (buffer == NULL) {
        return -1;
    }

    memset(buffer, 0, NVME_PAGE_SIZE);

    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_IDENTIFY;
    cmd.nsid       = nsid;
    cmd.prp1       = buffer_phys;
    cmd.cdw10      = 0;

    int ret = nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
    if (ret == 0) {
        memcpy(id_ns, buffer, sizeof(*id_ns));
    }

    g_nvme_platform_ops->dma_free(buffer, NVME_PAGE_SIZE);
    return ret;
}

static int nvme_create_io_cq(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_CREATE_CQ;
    cmd.prp1       = queue->cq_phys;
    cmd.cdw10      = ((queue->queue_depth - 1) << 16) | queue->queue_id;
    cmd.cdw11      = 1;

    return nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
}

static int nvme_create_io_sq(nvme_controller_t *ctrl, nvme_queue_t *queue) {
    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = NVME_ADMIN_CREATE_SQ;
    cmd.prp1       = queue->sq_phys;
    cmd.cdw10      = ((queue->queue_depth - 1) << 16) | queue->queue_id;
    cmd.cdw11      = ((uint32_t)queue->queue_id << 16) | 0x1;

    return nvme_admin_cmd_sync(ctrl, &cmd, NULL, 5000);
}

static int nvme_check_pci_config(pci_device_t *device) {
    if (device == NULL || device->op == NULL) {
        return -1;
    }

    if (!device->bars[0].address || device->bars[0].size < 0x2000) {
        g_nvme_platform_ops->log(
            "NVMe: Invalid BAR0 addr=0x%llx size=0x%llx\n",
            (unsigned long long)device->bars[0].address,
            (unsigned long long)device->bars[0].size
        );
        return -1;
    }

    if (!device->bars[0].mmio) {
        g_nvme_platform_ops->log("NVMe: BAR0 is not MMIO\n");
        return -1;
    }

    uint32_t command = device->op->read(
        device->bus, device->slot, device->func, device->segment, PCI_CONF_COMMAND
    );
    uint32_t new_command = (command & 0xFFFF) | (1U << 1) | (1U << 2);

    if ((new_command & 0xFFFF) != (command & 0xFFFF)) {
        device->op->write(
            device->bus, device->slot, device->func, device->segment, PCI_CONF_COMMAND, new_command
        );
    }

    return 0;
}

static inline uint32_t nvme_calc_num_pages(uint64_t addr, uint32_t size) {
    uint64_t end = addr + size - 1;
    return (end >> 12) - (addr >> 12) + 1;
}

static inline uint32_t nvme_page_offset(uint64_t addr) {
    return addr & NVME_PAGE_MASK;
}

static int nvme_prepare_prp_list(nvme_request_t *req) {
    if (req->prp_list) {
        return 0;
    }

    req->prp_list = g_nvme_platform_ops->dma_alloc(sizeof(nvme_prp_list_t), &req->prp_list_phys);
    if (req->prp_list == NULL) {
        return -1;
    }

    memset(req->prp_list, 0, sizeof(*req->prp_list));
    return 0;
}

static uint64_t nvme_translate_page_phys(uint64_t page_va) {
    return arch_virt_to_phys(page_va);
}

static int nvme_setup_prp(
    nvme_controller_t *ctrl,
    nvme_request_t *req,
    nvme_sqe_t *cmd,
    const void *buffer,
    uint64_t buffer_phys,
    uint32_t size
) {
    if (buffer == NULL || size == 0) {
        return -1;
    }
    if (ctrl->max_transfer_size > 0 && size > ctrl->max_transfer_size) {
        return -1;
    }

    uint64_t vaddr        = (uint64_t)buffer;
    uint64_t page_base_va = vaddr & ~((uint64_t)NVME_PAGE_MASK);
    uint32_t page_off     = nvme_page_offset(vaddr);
    uint32_t num_pages    = nvme_calc_num_pages(vaddr, size);

    uint64_t first_page_phys = nvme_translate_page_phys(page_base_va);
    if (buffer_phys) {
        uint64_t hinted = buffer_phys & ~((uint64_t)NVME_PAGE_MASK);
        if (hinted == first_page_phys) {
            first_page_phys = hinted;
        }
    }

    if (!first_page_phys) {
        printk("NVMe: first PRP page not mapped, vaddr=%#018llx\n", (unsigned long long)vaddr);
        return -1;
    }

    cmd->prp1 = first_page_phys + page_off;
    cmd->prp2 = 0;

    if (num_pages == 1) {
        return 0;
    }

    uint64_t second_page_phys = nvme_translate_page_phys(page_base_va + NVME_PAGE_SIZE);
    if (!second_page_phys) {
        printk("NVMe: second PRP page not mapped, vaddr=%#018llx\n", (unsigned long long)vaddr);
        return -1;
    }

    if (num_pages == 2) {
        cmd->prp2 = second_page_phys;
        return 0;
    }

    if (num_pages - 1 > NVME_MAX_PRP_LIST_ENTRIES) {
        printk("NVMe: PRP page count too large (%u pages)\n", num_pages);
        return -1;
    }

    if (nvme_prepare_prp_list(req) != 0) {
        printk("NVMe: failed to allocate request PRP list\n");
        return -1;
    }

    memset(req->prp_list, 0, sizeof(*req->prp_list));
    cmd->prp2             = req->prp_list_phys;
    req->prp_list->prp[0] = second_page_phys;

    for (uint32_t i = 2; i < num_pages; i++) {
        uint64_t pa = nvme_translate_page_phys(page_base_va + ((uint64_t)i * NVME_PAGE_SIZE));
        if (!pa) {
            printk(
                "NVMe: PRP page %u not mapped, vaddr=%#018llx\n",
                i,
                (unsigned long long)(page_base_va + ((uint64_t)i * NVME_PAGE_SIZE))
            );
            return -1;
        }
        req->prp_list->prp[i - 1] = pa;
    }

    g_nvme_platform_ops->wmb();
    return 0;
}

static nvme_queue_t *nvme_pick_io_queue(nvme_controller_t *ctrl) {
    (void)ctrl;
    return &ctrl->io_queues[0];
}

static int nvme_submit_io_async(
    nvme_controller_t *ctrl,
    uint8_t opcode,
    uint32_t nsid,
    uint64_t lba,
    uint32_t block_count,
    void *buffer,
    uint64_t buffer_phys,
    nvme_io_callback_t callback,
    void *ctx
) {
    if (ctrl == NULL || !ctrl->initialized || buffer == NULL || block_count == 0) {
        return -1;
    }
    if (nsid == 0 || nsid > ctrl->num_namespaces) {
        return -1;
    }

    nvme_namespace_t *ns = &ctrl->namespaces[nsid - 1];
    if (!ns->valid || ns->block_size == 0 || ns->block_count == 0) {
        return -1;
    }

    if (block_count - 1 > UINT16_MAX) {
        return -1;
    }
    if (lba >= ns->block_count || block_count > (ns->block_count - lba)) {
        return -1;
    }

    uint64_t transfer_size_u64 = (uint64_t)block_count * ns->block_size;
    if (transfer_size_u64 == 0 || transfer_size_u64 > UINT32_MAX) {
        return -1;
    }

    uint32_t transfer_size = (uint32_t)transfer_size_u64;
    if (ctrl->max_transfer_size > 0 && transfer_size > ctrl->max_transfer_size) {
        return -1;
    }

    uint16_t cid = nvme_alloc_cid(ctrl, callback, ctx);
    if (cid == UINT16_MAX) {
        return -1;
    }

    nvme_request_t *req = &ctrl->request_slots[cid];

    nvme_sqe_t cmd = { 0 };
    cmd.cdw0       = opcode | ((uint32_t)cid << 16);
    cmd.nsid       = nsid;
    cmd.cdw10      = (uint32_t)(lba & 0xFFFFFFFF);
    cmd.cdw11      = (uint32_t)(lba >> 32);
    cmd.cdw12      = (block_count - 1) & 0xFFFF;

    if (nvme_setup_prp(ctrl, req, &cmd, buffer, buffer_phys, transfer_size) != 0) {
        nvme_release_cid(ctrl, cid);
        printk("NVMe: setting up PRP failed\n");
        return -1;
    }

    if (opcode == NVME_CMD_WRITE) {
        g_nvme_platform_ops->wmb();
    }

    nvme_queue_t *queue = nvme_pick_io_queue(ctrl);
    if (nvme_submit_cmd(queue, &cmd) != 0) {
        nvme_release_cid(ctrl, cid);
        printk("NVMe: submit command failed\n");
        return -1;
    }

    return 0;
}

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
    return nvme_submit_io_async(
        ctrl, NVME_CMD_READ, nsid, lba, block_count, buffer, buffer_phys, callback, ctx
    );
}

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
    return nvme_submit_io_async(
        ctrl, NVME_CMD_WRITE, nsid, lba, block_count, (void *)buffer, buffer_phys, callback, ctx
    );
}

static void nvme_io_callback(void *ctx, bool success, uint32_t result) {
    nvme_callback_ctx_t *cb_ctx = ctx;
    cb_ctx->success             = success;
    cb_ctx->result              = result;
    __atomic_store_n(&cb_ctx->completed, true, __ATOMIC_RELEASE);
}

static uint64_t nvme_wait_io_done(
    nvme_controller_t *ctrl,
    nvme_queue_t *queue,
    nvme_callback_ctx_t *cb_ctx,
    uint64_t ok_ret,
    const char *op_name,
    uint32_t timeout_ms
) {
    uint64_t start = g_nvme_platform_ops->get_time_ms();

    while (!__atomic_load_n(&cb_ctx->completed, __ATOMIC_ACQUIRE)) {
        if (!nvme_process_queue_completions(ctrl, queue)) {
            arch_pause();
        }

        if (timeout_ms != (uint32_t)-1 && g_nvme_platform_ops->get_time_ms() - start > timeout_ms) {
            printk("NVMe: %s command timed out after %u ms\n", op_name, timeout_ms);
            nvme_dump_status(ctrl);
            return 0;
        }
    }

    bool success    = cb_ctx->success;
    uint32_t result = cb_ctx->result;

    if (success) {
        return ok_ret;
    }

    printk("NVMe: %s command failed, result=0x%08x\n", op_name, result);
    return 0;
}

static size_t nvme_read(void *data, uint8_t *buffer, size_t size, size_t lba) {
    if (size == 0 || size > UINT32_MAX) {
        return 0;
    }

    nvme_ns_t *ns               = data;
    nvme_queue_t *queue         = nvme_pick_io_queue(ns->ctrl);
    nvme_callback_ctx_t cb_ctx = {0};

    if (nvme_read_async(
            ns->ctrl, ns->ns->nsid, lba, (uint32_t)size, buffer, 0, nvme_io_callback, &cb_ctx
        )
        != 0) {
        printk("NVMe: submit read command failed\n");
        return 0;
    }

    return nvme_wait_io_done(ns->ctrl, queue, &cb_ctx, size, "read", 30000);
}

static size_t nvme_write(void *data, uint8_t *buffer, size_t size, size_t lba) {
    if (size == 0 || size > UINT32_MAX) {
        return 0;
    }

    nvme_ns_t *ns               = data;
    nvme_queue_t *queue         = nvme_pick_io_queue(ns->ctrl);
    nvme_callback_ctx_t cb_ctx = {0};

    if (nvme_write_async(
            ns->ctrl, ns->ns->nsid, lba, (uint32_t)size, buffer, 0, nvme_io_callback, &cb_ctx
        )
        != 0) {
        printk("NVMe: submit write command failed\n");
        return 0;
    }

    return nvme_wait_io_done(ns->ctrl, queue, &cb_ctx, size, "write", 30000);
}

static void nvme_destroy_controller(nvme_controller_t *ctrl) {
    if (ctrl == NULL) {
        return;
    }

    for (uint32_t i = 0; i < NVME_MAX_REQUESTS; i++) {
        nvme_request_t *req = &ctrl->request_slots[i];
        if (req->prp_list) {
            g_nvme_platform_ops->dma_free(req->prp_list, sizeof(nvme_prp_list_t));
            req->prp_list      = NULL;
            req->prp_list_phys = 0;
        }
    }

    for (uint32_t i = 0; i < ctrl->num_io_queues && i < NVME_MAX_IO_QUEUES; i++) {
        nvme_free_queue(&ctrl->io_queues[i]);
    }
    nvme_free_queue(&ctrl->admin_queue);

    g_nvme_platform_ops->dma_free(ctrl, sizeof(*ctrl));
}

static uint32_t nvme_calc_max_transfer_size(const nvme_identify_ctrl_t *id_ctrl) {
    uint64_t max_transfer = (uint64_t)NVME_MAX_PRP_LIST_ENTRIES * NVME_PAGE_SIZE;

    if (id_ctrl->mdts) {
        uint64_t controller_limit = ((uint64_t)1 << id_ctrl->mdts) * PAGE_SIZE;
        max_transfer              = MIN(max_transfer, controller_limit);
    }

    if (max_transfer > UINT32_MAX) {
        return UINT32_MAX;
    }

    return (uint32_t)max_transfer;
}

static void nvme_register_namespace(
    nvme_controller_t *ctrl, uint64_t controller_id, nvme_namespace_t *ns_info
) {
    nvme_ns_t *ns        = malloc(sizeof(*ns));
    blk_device_t *device = malloc(sizeof(*device));

    if (ns == NULL || device == NULL) {
        free(ns);
        free(device);
        return;
    }

    memset(device, 0, sizeof(*device));

    ns->ctrl = ctrl;
    ns->ns   = ns_info;

    char name_buf[sizeof(device->name)] = { 0 };
    sprintf(name_buf, "nvme%llun%u", (unsigned long long)controller_id, ns_info->nsid);

    device->handle           = ns;
    device->size             = ns_info->block_count * ns_info->block_size;
    device->block_size       = ns_info->block_size;
    device->max_size         = MAX(ctrl->max_transfer_size, ns_info->block_size);
    device->type             = BLK_BLOCK_DEVICE;
    device->ops.read         = nvme_read;
    device->ops.write        = nvme_write;
    device->ops.ioctl        = (void *)dummy;
    device->ops.poll         = (void *)dummy;
    device->ops.map          = (void *)dummy;
    device->geometry.heads   = 64;
    device->geometry.sectors = 32;

    uint64_t cyls =
        ns_info->block_count / ((uint64_t)(device->geometry.heads * device->geometry.sectors));
    device->geometry.cylinders = cyls > 65535 ? 65535 : (unsigned short)cyls;

    strcpy(device->name, name_buf);

    const size_t device_id = register_device(device);
    printk(
        "NVME: %s: blk_size=%u, blk=0..%llu, device_id=%llu\n",
        name_buf,
        ns_info->block_size,
        (unsigned long long)(ns_info->block_count ? ns_info->block_count - 1 : 0),
        (unsigned long long)device_id
    );
}

static int nvme_probe_device(pci_device_t *device) {
    if (g_nvme_platform_ops == NULL) {
        return -1;
    }
    if (nvme_check_pci_config(device) != 0) {
        return -1;
    }

    g_nvme_platform_ops->log(
        "NVMe: Probing device %04x:%04x\n", device->vendor_id, device->device_id
    );

    nvme_controller_t *ctrl = g_nvme_platform_ops->dma_alloc(sizeof(*ctrl), NULL);
    if (ctrl == NULL) {
        return -1;
    }

    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->cid_alloc_lock = SPIN_INIT;
    ctrl->pci_dev        = device;
    ctrl->bar0           = phys_to_virt(device->bars[0].address);

    if (ctrl->bar0 == NULL) {
        g_nvme_platform_ops->log("NVMe: BAR0 not mapped\n");
        goto error;
    }

    page_map_range(
        get_kernel_pagedir(),
        (uint64_t)ctrl->bar0,
        device->bars[0].address,
        PADDING_UP(device->bars[0].size, PAGE_SIZE),
        get_kernel_pte_flags()
    );

    uint64_t cap    = NVME_READ64(ctrl, NVME_REG_CAP);
    uint32_t mpsmin = (cap >> 48) & 0xF;
    uint32_t mpsmax = (cap >> 52) & 0xF;

    ctrl->doorbell_stride = 4 << ((cap >> 32) & 0xF);

    g_nvme_platform_ops->log(
        "NVMe: CAP=%016llx doorbell_stride=%u mpsmin=%u mpsmax=%u\n",
        (unsigned long long)cap,
        ctrl->doorbell_stride,
        mpsmin,
        mpsmax
    );

    if (mpsmin != 0) {
        g_nvme_platform_ops->log(
            "NVMe: Unsupported controller minimum page size %u KiB\n", 1U << mpsmin
        );
        goto error;
    }

    if (nvme_disable_controller(ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to disable controller\n");
        goto error;
    }

    if (nvme_init_queue(ctrl, &ctrl->admin_queue, 0, NVME_ADMIN_QUEUE_SIZE) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to initialize admin queue\n");
        goto error;
    }

    NVME_WRITE32(
        ctrl, NVME_REG_AQA, ((NVME_ADMIN_QUEUE_SIZE - 1) << 16) | (NVME_ADMIN_QUEUE_SIZE - 1)
    );
    NVME_WRITE64(ctrl, NVME_REG_ASQ, ctrl->admin_queue.sq_phys);
    NVME_WRITE64(ctrl, NVME_REG_ACQ, ctrl->admin_queue.cq_phys);

    if (nvme_enable_controller(ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to enable controller\n");
        goto error;
    }

    nvme_identify_ctrl_t id_ctrl;
    if (nvme_identify_controller(ctrl, &id_ctrl) != 0) {
        g_nvme_platform_ops->log("NVMe: Failed to identify controller\n");
        goto error;
    }

    ctrl->num_namespaces    = MIN((uint32_t)NVME_MAX_NAMESPACES, id_ctrl.nn);
    ctrl->max_transfer_size = nvme_calc_max_transfer_size(&id_ctrl);
    ctrl->num_io_queues     = 1;
    ctrl->page_size         = NVME_PAGE_SIZE;

    g_nvme_platform_ops->log(
        "NVMe: Model=%.40s Namespaces=%u MaxTransfer=%u\n",
        id_ctrl.mn,
        ctrl->num_namespaces,
        ctrl->max_transfer_size
    );

    for (uint32_t qid = 0; qid < ctrl->num_io_queues; qid++) {
        if (nvme_init_queue(ctrl, &ctrl->io_queues[qid], 1 + qid, NVME_IO_QUEUE_SIZE) != 0) {
            g_nvme_platform_ops->log("NVMe: Failed to initialize I/O queue\n");
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

    uint64_t controller_id = nvme_controller_index++;
    for (uint32_t nsid = 1; nsid <= ctrl->num_namespaces; nsid++) {
        nvme_identify_ns_t id_ns;
        if (nvme_identify_namespace(ctrl, nsid, &id_ns) != 0 || id_ns.nsze == 0) {
            continue;
        }

        nvme_namespace_t *ns_info = &ctrl->namespaces[nsid - 1];
        ns_info->nsid             = nsid;
        ns_info->block_count      = id_ns.nsze;

        uint8_t lba_format = id_ns.flbas & 0xF;
        if (lba_format < 16 && id_ns.lbaf[lba_format].lbads < 32) {
            ns_info->block_size = 1U << id_ns.lbaf[lba_format].lbads;
        }

        if (ns_info->block_size == 0) {
            g_nvme_platform_ops->log("NVMe: NS%u has invalid block size, skipping\n", nsid);
            continue;
        }

        ns_info->valid = true;

        g_nvme_platform_ops->log(
            "NVMe: NS%u: %llu blocks x %u bytes\n",
            nsid,
            (unsigned long long)id_ns.nsze,
            ns_info->block_size
        );

        nvme_register_namespace(ctrl, controller_id, ns_info);
    }

    device->desc = ctrl;
    g_nvme_platform_ops->log("NVMe: Initialization complete\n");
    return 0;

error:
    nvme_dump_status(ctrl);
    nvme_destroy_controller(ctrl);
    return -1;
}

void nvme_probe(pci_device_t *device) {
    (void)nvme_probe_device(device);
}

void nvme_process_completions(nvme_controller_t *ctrl) {
    if (ctrl == NULL) {
        return;
    }

    while (nvme_process_queue_completions(ctrl, &ctrl->admin_queue)) {
    }
    for (uint32_t i = 0; i < ctrl->num_io_queues && i < NVME_MAX_IO_QUEUES; i++) {
        while (nvme_process_queue_completions(ctrl, &ctrl->io_queues[i])) {
        }
    }
}

int nvme_get_namespace_info(
    nvme_controller_t *ctrl, uint32_t nsid, uint64_t *block_count, uint32_t *block_size
) {
    if (ctrl == NULL || nsid == 0 || nsid > ctrl->num_namespaces) {
        return -1;
    }

    nvme_namespace_t *ns = &ctrl->namespaces[nsid - 1];
    if (!ns->valid) {
        return -1;
    }

    if (block_count) {
        *block_count = ns->block_count;
    }
    if (block_size) {
        *block_size = ns->block_size;
    }

    return 0;
}

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    nvme_set_platform_ops(&cpkrnl_nvme_platform_ops);
    pci_find_class(0x00010802, nvme_probe);
    return EOK;
}
