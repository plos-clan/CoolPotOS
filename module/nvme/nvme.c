#include "nvme.h"
#include "driver_subsystem.h"
#include "mem_subsystem.h"

NVME_NAMESPACE *namespace_[256];

void failed_nvme(NVME_CONTROLLER *ctrl) {
    if (ctrl->ICQ.CQE) {
        unmap_page_range(get_current_directory(), (uint64_t)virt_to_phys((uint64_t)ctrl->ICQ.CQE),
                         PAGE_SIZE);
    }
    if (ctrl->ISQ.SQE) {
        unmap_page_range(get_current_directory(), (uint64_t)virt_to_phys((uint64_t)ctrl->ISQ.SQE),
                         PAGE_SIZE);
    }
    if (ctrl->ACQ.CQE) {
        unmap_page_range(get_current_directory(), (uint64_t)virt_to_phys((uint64_t)ctrl->ACQ.CQE),
                         PAGE_SIZE);
    }
    if (ctrl->ASQ.SQE) {
        unmap_page_range(get_current_directory(), (uint64_t)virt_to_phys((uint64_t)ctrl->ASQ.SQE),
                         PAGE_SIZE);
    }
}

void NVMEConfigureQ(NVME_CONTROLLER *ctrl, NVME_QUEUE_COMMON *q, uint32_t idx, uint32_t len) {
    memset(q, 0, sizeof(NVME_QUEUE_COMMON));
    q->DBL = (uint32_t *)(((uint8_t *)ctrl->CAP) + 0x1000 + idx * ctrl->DST);
    q->MSK = len - 1;
}

int NVMEConfigureCQ(NVME_CONTROLLER *ctrl, NVME_COMPLETION_QUEUE *cq, uint32_t idx, uint32_t len) {
    NVMEConfigureQ(ctrl, &cq->COM, idx, len);
    cq->CQE          = 0;
    uint64_t phyAddr = 0;
    phyAddr          = (uint64_t)alloc_frames(1);
    cq->CQE          = (NVME_COMPLETION_QUEUE_ENTRY *)driver_phys_to_virt(phyAddr);
    page_map_range(get_current_directory(), (uint64_t)cq->CQE, phyAddr, PAGE_SIZE,
                   KERNEL_PTE_FLAGS);
    memset(cq->CQE, 0, 0x1000);
    cq->COM.HAD = 0;
    cq->COM.TAL = 0;
    cq->COM.PHA = 1;
    return 0;
}

int NVMEConfigureSQ(NVME_CONTROLLER *ctrl, NVME_SUBMISSION_QUEUE *sq, uint32_t idx, uint32_t len) {
    NVMEConfigureQ(ctrl, &sq->COM, idx, len);
    uint64_t phyAddr = 0;
    phyAddr          = (uint64_t)alloc_frames(1);
    sq->SQE          = (NVME_SUBMISSION_QUEUE_ENTRY *)driver_phys_to_virt(phyAddr);
    page_map_range(get_current_directory(), (uint64_t)sq->SQE, phyAddr, PAGE_SIZE,
                   KERNEL_PTE_FLAGS);
    memset(sq->SQE, 0, 0x1000);
    sq->COM.HAD = 0;
    sq->COM.TAL = 0;
    sq->COM.PHA = 0;
    return 0;
}

int NVMEWaitingRDY(NVME_CONTROLLER *ctrl, uint32_t rdy) {
    uint32_t csts;
    while (rdy != ((csts = ctrl->CAP->CST) & NVME_CSTS_RDY)) {
        __asm__ volatile("pause");
    }
    return 0;
}

NVME_COMPLETION_QUEUE_ENTRY NVMEWaitingCMD(NVME_SUBMISSION_QUEUE       *sq,
                                           NVME_SUBMISSION_QUEUE_ENTRY *e) {
    NVME_COMPLETION_QUEUE_ENTRY errcqe;
    memset(&errcqe, 0xFF, sizeof(NVME_COMPLETION_QUEUE_ENTRY));

    if (((sq->COM.TAL + 1) % (sq->COM.MSK + 1ULL)) == sq->COM.HAD) {
        printk("SUBMISSION QUEUE IS FULL\n");
        return errcqe;
    }

    // Commit
    NVME_SUBMISSION_QUEUE_ENTRY *sqe = sq->SQE + sq->COM.TAL;
    memcpy(sqe, e, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe->CDW0 |= (uint32_t)sq->COM.TAL << 16;

    // Doorbell
    sq->COM.TAL++;
    sq->COM.TAL    %= (sq->COM.MSK + 1ULL);
    sq->COM.DBL[0]  = (uint32_t)sq->COM.TAL;

    // Check completion
    NVME_COMPLETION_QUEUE *cq = sq->ICQ;
    while ((cq->CQE[cq->COM.HAD].STS & 0x1) != cq->COM.PHA) {
        __asm__ volatile("pause");
    }

    // Consume CQE
    NVME_COMPLETION_QUEUE_ENTRY *cqe       = cq->CQE + cq->COM.HAD;
    uint16_t                     cqNextHAD = (cq->COM.HAD + 1) % (cq->COM.MSK + 1ULL);
    if (cqNextHAD < cq->COM.HAD) { cq->COM.PHA ^= 1; }
    cq->COM.HAD = cqNextHAD;

    if (cqe->QHD != sq->COM.HAD) { sq->COM.HAD = cqe->QHD; }
    // Doorbell
    cq->COM.DBL[0] = (uint32_t)cq->COM.HAD;
    return *cqe;
}

void failed_namespace(NVME_IDENTIFY_NAMESPACE *identifyNS) {
    unmap_page_range(get_current_directory(), (uint64_t)identifyNS, PAGE_SIZE);
}

bool BuildPRPList(void *vaddr, uint64_t size, NVME_PRP_LIST *prpList) {
    uint64_t vaddrStart = (uint64_t)vaddr;
    uint64_t vaddrEnd   = vaddrStart + size;
    uint64_t pageMask   = PAGE_SIZE - 1;

    uint64_t firstPage = vaddrStart & ~pageMask;
    uint64_t lastPage  = (vaddrEnd - 1) & ~pageMask;
    uint32_t pageCount = ((lastPage - firstPage) / PAGE_SIZE) + 1;

    if (pageCount <= 2) {
        prpList->prp1 = page_virt_to_phys(vaddrStart);
        if (pageCount == 1) {
            prpList->prp2 = 0;
        } else {
            prpList->prp2 = page_virt_to_phys(firstPage + PAGE_SIZE);
        }
        prpList->UPRP = false;
        prpList->A    = NULL;
        return true;
    }

    uint32_t prpEntries = pageCount - 1;

    size_t   phy_len  = prpEntries * sizeof(uint64_t);
    uint64_t prp_phy0 = alloc_frames(phy_len / PAGE_SIZE + 1);
    if (!prp_phy0) return false;
    uint64_t *prpArray = (uint64_t *)driver_phys_to_virt(prp_phy0);
    page_map_range(get_current_directory(), (uint64_t)prpArray, prp_phy0, phy_len,
                   KERNEL_PTE_FLAGS);

    uint64_t currentPage = firstPage + PAGE_SIZE;
    for (uint32_t i = 0; i < prpEntries; i++) {
        prpArray[i]  = page_virt_to_phys(currentPage);
        currentPage += PAGE_SIZE;
    }

    prpList->prp1 = page_virt_to_phys(vaddrStart);
    prpList->prp2 = page_virt_to_phys((uint64_t)prpArray);
    prpList->UPRP = true;
    prpList->A    = prpArray;
    prpList->S    = prpEntries * sizeof(uint64_t);
    return true;
}

void FreePRPList(NVME_PRP_LIST *prpList) {
    if (prpList->A) {
        unmap_page_range(get_current_directory(), (uint64_t)prpList->A, prpList->S);
        prpList->A = NULL;
    }
}

void nvme_rwfail(uint16_t status) {
    printk("Status: %#010lx, status code: %#010lx, status code type: %#010lx\n", status,
           status & 0xFF, (status >> 8) & 0x7);
}

uint32_t NVMETransfer(NVME_NAMESPACE *ns, void *buf, uint64_t lba, uint32_t count, uint32_t write) {
    if (!count || !ns || !buf) return 0;

    if (lba + count > ns->NLBA) {
        printk("NVME: LBA out of range (lba=%llu, count=%u, max=%llu)\n", lba, count, ns->NLBA);
        return 0;
    }

    uint32_t transferred = 0;
    uint8_t *currentBuf  = (uint8_t *)buf;
    uint64_t currentLBA  = lba;

    while (count > 0) {
        uint32_t chunk = count;
        if (ns->MXRS != (uint32_t)-1 && chunk > ns->MXRS) { chunk = ns->MXRS; }

        uint64_t      size = chunk * ns->BSZ;
        NVME_PRP_LIST prpList;
        if (!BuildPRPList(currentBuf, size, &prpList)) {
            printk("nvme: Failed to build PRP list\n");
            return transferred;
        }

        NVME_SUBMISSION_QUEUE_ENTRY sqe;
        memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        sqe.CDW0 = write ? NVME_SQE_OPC_IO_WRITE : NVME_SQE_OPC_IO_READ;
        sqe.NSID = ns->NSID;
        sqe.CDWA = (uint32_t)(currentLBA & 0xFFFFFFFF);
        sqe.CDWB = (uint32_t)(currentLBA >> 32);
        sqe.CDWC = (1UL << 31) | ((chunk - 1) & 0xFFFF);

        sqe.DATA[0] = prpList.prp1;
        sqe.DATA[1] = prpList.prp2;
        if (prpList.UPRP) { sqe.CDW0 |= NVME_SQE_PRP_PTR; }

        NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&ns->CTRL->ISQ, &sqe);
        if ((cqe.STS >> 1) & 0xFF) {
            nvme_rwfail(cqe.STS);
            FreePRPList(&prpList);
            return transferred;
        }

        currentBuf  += size;
        currentLBA  += chunk;
        transferred += chunk;
        count       -= chunk;

        FreePRPList(&prpList);
    }

    return transferred;
}

size_t nvme_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    NVME_NAMESPACE *data = namespace_[drive];
    return NVMETransfer(data, buffer, lba, number, false);
}

size_t nvme_write(int drive, uint8_t *buffer, size_t number, size_t lba) {
    NVME_NAMESPACE *data = namespace_[drive];
    return NVMETransfer(data, buffer, lba, number, true);
}

static int empty() {
    return 0;
}

NVME_CONTROLLER *nvme_driver_init(uint64_t bar0, uint64_t bar_size) {
    NVME_CAPABILITY *cap = (NVME_CAPABILITY *)bar0;
    if (!((cap->CAP >> 37) & 1)) {
        printk("nvme: NVME CONTROLLER DOES NOT SUPPORT NVME COMMAND SET\n");
        return NULL;
    }

    NVME_CONTROLLER *ctrl = (NVME_CONTROLLER *)malloc(sizeof(NVME_CONTROLLER));
    memset(ctrl, 0, sizeof(NVME_CONTROLLER));
    ctrl->CAP = cap;
    ctrl->WTO = 500 * ((cap->CAP >> 24) & 0xFF);

    // RST controller
    ctrl->CAP->CC = 0;
    if (NVMEWaitingRDY(ctrl, 0)) {
        printk("nvme: NVME FATAL ERROR DURING CONTROLLER SHUTDOWN\n");
        failed_nvme(ctrl);
        return NULL;
    }
    ctrl->DST = 4ULL << ((cap->CAP >> 32) & 0xF);

    int rc = NVMEConfigureCQ(ctrl, &ctrl->ACQ, 1, 0x1000 / sizeof(NVME_COMPLETION_QUEUE_ENTRY));
    if (rc) {
        failed_nvme(ctrl);
        return NULL;
    }

    rc = NVMEConfigureSQ(ctrl, &ctrl->ASQ, 0, 0x1000 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    if (rc) {
        failed_nvme(ctrl);
        return NULL;
    }
    ctrl->ASQ.ICQ = &ctrl->ACQ;

    ctrl->CAP->AQA = ((uint32_t)ctrl->ACQ.COM.MSK << 16) | ctrl->ASQ.COM.MSK;
    ctrl->CAP->ASQ = (uint64_t)driver_virt_to_phys((uint64_t)ctrl->ASQ.SQE);
    ctrl->CAP->ACQ = (uint64_t)driver_phys_to_virt((uint64_t)ctrl->ACQ.CQE);

    ctrl->CAP->CC = 1 | (4 << 20) | (6 << 16);
    if (NVMEWaitingRDY(ctrl, 1)) {
        printk("nvme: NVME FATAL ERROR DURING CONTROLLER ENABLING");
        failed_nvme(ctrl);
        return NULL;
    }

    /* The admin queue is set up and the controller is ready. Let's figure out
       what namespaces we have. */
    // Identify Controller
    NVME_IDENTIFY_CONTROLLER *identify = 0;
    uint64_t                  phy0     = alloc_frames(1);
    identify = (NVME_IDENTIFY_CONTROLLER *)driver_phys_to_virt((uint64_t)identify);
    page_map_range(get_current_directory(), (uint64_t)identify, phy0, PAGE_SIZE, KERNEL_PTE_FLAGS);

    memset(identify, 0, PAGE_SIZE);

    NVME_SUBMISSION_QUEUE_ENTRY sqe;
    memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0                        = NVME_SQE_OPC_ADMIN_IDENTIFY;
    sqe.META                        = 0;
    sqe.DATA[0]                     = (uint64_t)driver_virt_to_phys((uint64_t)identify);
    sqe.DATA[1]                     = 0;
    sqe.NSID                        = 0;
    sqe.CDWA                        = NVME_ADMIN_IDENTIFY_CNS_ID_CTRL;
    NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&ctrl->ASQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF) {
        printk("CANNOT IDENTIFY NVME CONTROLLER\n");
        failed_nvme(ctrl);
        return NULL;
    }

    char buf[41];
    memcpy(buf, identify->SERN, sizeof(identify->SERN));
    buf[sizeof(identify->SERN)] = 0;
    char *serialN               = LeadingWhitespace(buf, buf + sizeof(identify->SERN));
    memcpy(ctrl->SER, serialN, strlen(serialN));

    memcpy(buf, identify->MODN, sizeof(identify->MODN));
    buf[sizeof(identify->MODN)] = 0;
    serialN                     = LeadingWhitespace(buf, buf + sizeof(identify->MODN));
    memcpy(ctrl->MOD, serialN, strlen(serialN));

    ctrl->NSC    = identify->NNAM;
    uint8_t mdts = identify->MDTS;
    unmap_page_range(get_current_directory(), (uint64_t)driver_virt_to_phys((uint64_t)identify),
                     PAGE_SIZE);

    if (ctrl->NSC == 0) {
        printk("NO NAMESPACE\n");
        failed_nvme(ctrl);
        return NULL;
    }

    // Create I/O Queue
    // Create I/O CQ
    {
        uint32_t qidx       = 3;
        uint32_t entryCount = 1 + (ctrl->CAP->CAP & 0xFFFF);
        if (entryCount > 0x1000 / sizeof(NVME_COMPLETION_QUEUE_ENTRY))
            entryCount = 0x1000 / sizeof(NVME_COMPLETION_QUEUE_ENTRY);
        if (NVMEConfigureCQ(ctrl, &ctrl->ICQ, qidx, entryCount)) {
            printk("CANNOT INIT I/O CQ\n");
            failed_nvme(ctrl);
            return NULL;
        }
        NVME_SUBMISSION_QUEUE_ENTRY ccq;
        memset(&ccq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        ccq.CDW0    = NVME_SQE_OPC_ADMIN_CREATE_IO_CQ;
        ccq.META    = 0;
        ccq.DATA[0] = (uint64_t)virt_to_phys((uint64_t)ctrl->ICQ.CQE);
        ccq.DATA[1] = 0;
        ccq.CDWA    = ((uint32_t)ctrl->ICQ.COM.MSK << 16) | (qidx >> 1);
        ccq.CDWB    = 1;

        cqe = NVMEWaitingCMD(&ctrl->ASQ, &ccq);
        if ((cqe.STS >> 1) & 0xFF) {
            printk("CANNOT CREATE I/O CQ\n");
            failed_nvme(ctrl);
            return NULL;
        }
    }

    // Create I/O SQ
    {
        uint32_t qidx       = 2;
        uint32_t entryCount = 1 + (ctrl->CAP->CAP & 0xFFFF);
        if (entryCount > 0x1000 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY))
            entryCount = 0x1000 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY);
        if (NVMEConfigureSQ(ctrl, &ctrl->ISQ, qidx, entryCount)) {
            printk("CANNOT INIT I/O SQ\n");
            failed_nvme(ctrl);
            return NULL;
        }
        NVME_SUBMISSION_QUEUE_ENTRY csq;
        memset(&csq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        csq.CDW0    = NVME_SQE_OPC_ADMIN_CREATE_IO_SQ;
        csq.META    = 0;
        csq.DATA[0] = (uint64_t)virt_to_phys((uint64_t)ctrl->ISQ.SQE);
        csq.DATA[1] = 0;
        csq.CDWA    = ((uint32_t)ctrl->ISQ.COM.MSK << 16) | (qidx >> 1);
        csq.CDWB    = ((qidx >> 1) << 16) | 1;

        cqe = NVMEWaitingCMD(&ctrl->ASQ, &csq);
        if ((cqe.STS >> 1) & 0xFF) {
            printk("CANNOT CREATE I/O SQ");
            failed_nvme(ctrl);
            return NULL;
        }
        ctrl->ISQ.ICQ = &ctrl->ICQ;
    }

    // /* Populate namespace IDs */
    // for (uint32_t nsidx = 0; nsidx < ctrl->NSC; nsidx++)
    // {

    uint32_t nsidx = 0;

    // Probe Namespace
    uint32_t nsid = nsidx + 1;

    uint64_t                 space_phy0 = alloc_frames(1);
    NVME_IDENTIFY_NAMESPACE *identifyNS =
        (NVME_IDENTIFY_NAMESPACE *)driver_phys_to_virt(space_phy0);
    page_map_range(get_current_directory(), (uint64_t)identifyNS, space_phy0, PAGE_SIZE,
                   KERNEL_PTE_FLAGS);

    memset(identifyNS, 0, 0x1000);

    memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0    = NVME_SQE_OPC_ADMIN_IDENTIFY;
    sqe.META    = 0;
    sqe.DATA[0] = (uint64_t)driver_virt_to_phys((uint64_t)identifyNS);
    sqe.DATA[1] = 0;
    sqe.NSID    = nsid;
    sqe.CDWA    = NVME_ADMIN_IDENTIFY_CNS_ID_NS;
    cqe         = NVMEWaitingCMD(&ctrl->ASQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF) {
        printk("CANNOT IDENTIFY NAMESPACE\n");
        failed_namespace(identifyNS);
        return NULL;
    }

    uint8_t currentLBAFormat = identifyNS->FLBA & 0xF;
    if (currentLBAFormat > identifyNS->NLBA) {
        printk("Current LBA Format error\n");
        failed_namespace(identifyNS);
        return NULL;
    }

    if (!identifyNS->SIZE) {
        printk("Invalid namespace size\n");
        failed_namespace(identifyNS);
        return NULL;
    }

    NVME_NAMESPACE *ns = (NVME_NAMESPACE *)malloc(sizeof(NVME_NAMESPACE));
    memset(ns, 0, sizeof(NVME_NAMESPACE));
    ns->CTRL = ctrl;
    ns->NSID = nsid;
    ns->NLBA = identifyNS->SIZE;

    NVME_LOGICAL_BLOCK_ADDRESS *fmt = identifyNS->LBAF + currentLBAFormat;

    ns->BSZ  = 1ULL << fmt->DS;
    ns->META = fmt->MS;
    if (ns->BSZ > 0x1000) {
        printk("BLOCK SIZE > 0x1000 !!!\n");
        failed_namespace(identifyNS);
        free(ns);
        return NULL;
    }

    if (mdts) {
        ns->MXRS = ((1ULL << mdts) * 0x1000) / ns->BSZ;
    } else {
        ns->MXRS = -1;
    }

    device_t nvme;
    nvme.type        = DEVICE_BLOCK;
    nvme.size        = ns->NLBA * ns->BSZ;
    nvme.map         = (void *)empty;
    nvme.read        = nvme_read;
    nvme.write       = nvme_write;
    nvme.ioctl       = (void *)empty;
    nvme.poll        = (void *)empty;
    nvme.flag        = 1;
    nvme.sector_size = ns->BSZ;
    strcpy(nvme.drive_name, "nvme0");
    namespace_[regist_device(nvme)] = ns;
    return ctrl;
}

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    pci_device_t *device = pci_find_class(0x10802);
    if (device == NULL) { return -ENODEV; }
    uint64_t virt_bar = (uint64_t)driver_phys_to_virt(device->bars[0].address);
    page_map_range(get_current_directory(), virt_bar, device->bars[0].address, device->bars[0].size,
                   KERNEL_PTE_FLAGS);
    NVME_CONTROLLER *nvme = nvme_driver_init(virt_bar, device->bars[0].size);
    if (!nvme) return -EIO;
    return EOK;
}
