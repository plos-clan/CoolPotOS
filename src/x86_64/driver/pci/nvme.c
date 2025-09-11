/**
 * XJ380 Nvme Driver for CP_Kernel
 * Copyright by xingji-studio 2025
 */
#include "nvme.h"
#include "device.h"
#include "heap.h"
#include "hhdm.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "page.h"
#include "pci.h"
#include "sprintf.h"
#include "timer.h"

void           *NVME_DMA_MEMORY = 0;
NVME_NAMESPACE *nvme_device[10];

int NVMEWaitingRDY(nvme_controller *ctrl, uint32_t rdy) {
    uint32_t csts;
    int      i = 0;
    while (rdy != ((csts = ctrl->CAP->CST) & NVME_CSTS_RDY)) {
        if (i > MAX_WAIT_INDEX) { return true; }
        __asm__ __volatile__("pause");
        i++;
        nsleep(1);
    }
    return 0;
}

void NVMEConfigureQ(nvme_controller *ctrl, NVME_QUEUE_COMMON *q, uint32_t idx, uint32_t len) {
    memset(q, 0, sizeof(NVME_QUEUE_COMMON));
    q->DBL = (uint32_t *)(((uint8_t *)ctrl->CAP) + PAGE_SIZE + idx * ctrl->DST);
    q->MSK = len - 1;
}

int NVMEConfigureCQ(nvme_controller *ctrl, NVME_COMPLETION_QUEUE *cq, uint32_t idx, uint32_t len) {
    NVMEConfigureQ(ctrl, &cq->COM, idx, len);
    cq->CQE          = 0;
    uint64_t phyAddr = 0;
    uint64_t pagCont = 1;
    phyAddr          = (uint64_t)malloc(pagCont * PAGE_SIZE);
    cq->CQE          = (NVME_COMPLETION_QUEUE_ENTRY *)phyAddr;
    memset(cq->CQE, 0, PAGE_SIZE);
    cq->COM.HAD = 0;
    cq->COM.TAL = 0;
    cq->COM.PHA = 1;
    return 0;
}

int NVMEConfigureSQ(nvme_controller *ctrl, NVME_SUBMISSION_QUEUE *sq, uint32_t idx, uint32_t len) {
    NVMEConfigureQ(ctrl, &sq->COM, idx, len);
    sq->SQE          = 0;
    uint64_t phyAddr = 0;
    uint64_t pagCont = 1;
    phyAddr          = (uint64_t)malloc(pagCont * PAGE_SIZE);
    sq->SQE          = (NVME_SUBMISSION_QUEUE_ENTRY *)phyAddr;
    memset(sq->SQE, 0, PAGE_SIZE);
    sq->COM.HAD = 0;
    sq->COM.TAL = 0;
    sq->COM.PHA = 0;
    return 0;
}

NVME_COMPLETION_QUEUE_ENTRY NVMEWaitingCMD(NVME_SUBMISSION_QUEUE       *sq,
                                           NVME_SUBMISSION_QUEUE_ENTRY *e) {
    NVME_COMPLETION_QUEUE_ENTRY errcqe;
    memset(&errcqe, 0xFF, sizeof(NVME_COMPLETION_QUEUE_ENTRY));

    if (((sq->COM.HAD + 1) % (sq->COM.MSK + 1ULL)) == sq->COM.TAL) {
        printk("SUBMISSION QUEUE IS FULL\n");
        return errcqe;
    }

    // Commit
    NVME_SUBMISSION_QUEUE_ENTRY *sqe = sq->SQE + sq->COM.TAL;
    memcpy(sqe, e, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe->CDW0 |= sq->COM.TAL << 16;

    // Doorbell
    sq->COM.TAL++;
    sq->COM.TAL    %= (sq->COM.MSK + 1ULL);
    sq->COM.DBL[0]  = sq->COM.TAL;

    // Check completion
    NVME_COMPLETION_QUEUE *cq = sq->ICQ;
    while ((cq->CQE[cq->COM.HAD].STS & 1) != cq->COM.PHA) {
        __asm__ __volatile__("pause");
    }

    // Consume CQE
    NVME_COMPLETION_QUEUE_ENTRY *cqe       = cq->CQE + cq->COM.HAD;
    uint16_t                     cqNextHAD = (cq->COM.HAD + 1) % (cq->COM.MSK + 1ULL);
    if (cqNextHAD < cq->COM.HAD) { cq->COM.PHA ^= 1; }
    cq->COM.HAD = cqNextHAD;

    if (cqe->QHD != sq->COM.HAD) { sq->COM.HAD = cqe->QHD; }
    // Doorbell
    cq->COM.DBL[0] = cq->COM.HAD;
    return *cqe;
}

uint32_t NVMETransfer(NVME_NAMESPACE *ns, void *buf, uint64_t lba, uint32_t count, uint32_t write) {
    if (!count) return 0;

    uint64_t bufAddr  = (uint64_t)buf;
    uint32_t maxCount = (PAGE_SIZE / ns->BSZ) - ((bufAddr & 0xFFF) / ns->BSZ);
    if (count > maxCount) count = maxCount;
    if (count > ns->MXRS) count = ns->MXRS;

    NVME_SUBMISSION_QUEUE_ENTRY sqe;
    memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0                        = write ? NVME_SQE_OPC_IO_WRITE : NVME_SQE_OPC_IO_READ;
    sqe.META                        = 0;
    sqe.DATA[0]                     = (uint64_t)virt_to_phys(bufAddr);
    sqe.DATA[1]                     = 0;
    sqe.NSID                        = ns->NSID;
    sqe.CDWA                        = lba;
    sqe.CDWB                        = lba >> 32;
    sqe.CDWC                        = (1UL << 31) | ((count - 1) & 0xFFFF);
    NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&ns->CTRL->ISQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF) {
        if (write) {
            kerror("Nvme cannot write\n");
        } else
            kerror("Nvme cannot read\n");
        return -1;
    }
    return count ? 0 : -1;
}

size_t NvmeRead(int id, uint8_t *buf, size_t count, size_t idx) {
    NVMETransfer(nvme_device[id], buf, idx, count, false);
    return count * SECTORS_ONCE;
}

size_t NvmeWrite(int id, uint8_t *buf, size_t count, size_t idx) {
    NVMETransfer(nvme_device[id], buf, idx, count, true);
    return count * SECTORS_ONCE;
}

void nvme_setup() {
    pci_device_t    *device = pci_find_class(0x10802);
    nvme_capability *capability;
    if (device == NULL) { return; }
    capability = (nvme_capability *)phys_to_virt(device->bars[0].address);

    page_map_range_to(get_kernel_pagedir(), device->bars[0].address, 0x1000 * 2, KERNEL_PTE_FLAGS);

    if (!((capability->CAP >> 37) & 1)) {
        kerror("Nvme controller does not support nvme command set.\n");
        return;
    }

    nvme_controller *controller = (nvme_controller *)malloc(sizeof(nvme_controller));
    memset(controller, 0, sizeof(nvme_controller));
    controller->DVC     = device;
    controller->CAP     = capability;
    controller->WTO     = 500 * ((capability->CAP >> 24) & 0xFF);
    controller->CAP->CC = 0;

    if (NVMEWaitingRDY(controller, 0)) {
        kerror("Nvme fatal error during conttoller shutdown.");
        return;
    }

    controller->DST = 4ULL << ((capability->CAP >> 32) & 0xF);
    int rc          = NVMEConfigureCQ(controller, &controller->ACQ, 1,
                                      PAGE_SIZE / sizeof(NVME_COMPLETION_QUEUE_ENTRY));
    if (rc) { return; }

    rc = NVMEConfigureSQ(controller, &controller->ASQ, 0,
                         PAGE_SIZE / sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    if (rc) { return; }
    controller->ASQ.ICQ = &controller->ACQ;

    controller->CAP->AQA = (controller->ACQ.COM.MSK << 16) | controller->ASQ.COM.MSK;
    controller->CAP->ASQ = (uint64_t)virt_to_phys((uint64_t)controller->ASQ.SQE);
    controller->CAP->ACQ = (uint64_t)virt_to_phys((uint64_t)controller->ACQ.CQE);

    controller->CAP->CC = 1 | (4 << 20) | (6 << 16);
    if (NVMEWaitingRDY(controller, 1)) {
        kerror("Nvme fatal error during controller enabling");
        return;
    }

    NVME_IDENTIFY_CONTROLLER *identify = malloc(sizeof(NVME_IDENTIFY_CONTROLLER));
    memset(identify, 0, sizeof(NVME_IDENTIFY_CONTROLLER));
    NVME_SUBMISSION_QUEUE_ENTRY sqe;
    memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0                        = NVME_SQE_OPC_ADMIN_IDENTIFY;
    sqe.META                        = 0;
    sqe.DATA[0]                     = (uint64_t)virt_to_phys((uint64_t)identify);
    sqe.DATA[1]                     = 0;
    sqe.NSID                        = 0;
    sqe.CDWA                        = NVME_ADMIN_IDENTIFY_CNS_ID_CTRL;
    NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&controller->ASQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF) {
        kerror("Cannot identify nvme controller.");
        return;
    }

    char buf[41];
    memcpy(buf, identify->SERN, sizeof(identify->SERN));
    buf[sizeof(identify->SERN)] = 0;
    char *serialN               = LeadingWhitespace(buf, buf + sizeof(identify->SERN));
    memcpy(controller->SER, serialN, strlen(serialN));
    memcpy(buf, identify->MODN, sizeof(identify->MODN));
    buf[sizeof(identify->MODN)] = 0;
    serialN                     = LeadingWhitespace(buf, buf + sizeof(identify->MODN));
    memcpy(controller->MOD, serialN, strlen(serialN));

    controller->NSC = identify->NNAM;
    uint8_t mdts    = identify->MDTS;
    free(identify);

    if (controller->NSC == 0) {
        kerror("Nvme no namespace");
        return;
    }

    { // Unsafe
        uint32_t qidx       = 3;
        uint32_t entryCount = 1 + (controller->CAP->CAP & 0xFFFF);
        if (entryCount > PAGE_SIZE / sizeof(NVME_COMPLETION_QUEUE_ENTRY))
            entryCount = PAGE_SIZE / sizeof(NVME_COMPLETION_QUEUE_ENTRY);
        if (NVMEConfigureCQ(controller, &controller->ICQ, qidx, entryCount)) {
            kerror("Nvme cannot init I/O CQ\n");
            return;
        }
        NVME_SUBMISSION_QUEUE_ENTRY ccq;
        memset(&ccq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        ccq.CDW0    = NVME_SQE_OPC_ADMIN_CREATE_IO_CQ;
        ccq.META    = 0;
        ccq.DATA[0] = (uint64_t)virt_to_phys((uint64_t)controller->ICQ.CQE);
        ccq.DATA[1] = 0;
        ccq.CDWA    = (controller->ICQ.COM.MSK << 16) | (qidx >> 1);
        ccq.CDWB    = 1;

        cqe = NVMEWaitingCMD(&controller->ASQ, &ccq);
        if ((cqe.STS >> 1) & 0xFF) {
            kerror("Nvme cannot create I/O CQ\n");
            return;
        }
    }

    { // Unsafe
        uint32_t qidx       = 2;
        uint32_t entryCount = 1 + (controller->CAP->CAP & 0xFFFF);
        if (entryCount > PAGE_SIZE / sizeof(NVME_SUBMISSION_QUEUE_ENTRY))
            entryCount = PAGE_SIZE / sizeof(NVME_SUBMISSION_QUEUE_ENTRY);
        if (NVMEConfigureSQ(controller, &controller->ISQ, qidx, entryCount)) {
            kerror("Nvme cannot init I/O SQ\n");
            return;
        }
        NVME_SUBMISSION_QUEUE_ENTRY csq;
        memset(&csq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        csq.CDW0    = NVME_SQE_OPC_ADMIN_CREATE_IO_SQ;
        csq.META    = 0;
        csq.DATA[0] = (uint64_t)virt_to_phys((uint64_t)controller->ISQ.SQE);
        csq.DATA[1] = 0;
        csq.CDWA    = (controller->ICQ.COM.MSK << 16) | (qidx >> 1);
        csq.CDWB    = ((qidx >> 1) << 16) | 1;

        cqe = NVMEWaitingCMD(&controller->ASQ, &csq);
        if ((cqe.STS >> 1) & 0xFF) {
            kerror("Nvme cannot create I/O SQ\n");
            return;
        }
        controller->ISQ.ICQ = &controller->ICQ;
    }

    for (uint32_t nsidx = 0; nsidx < controller->NSC; nsidx++) {
        // Probe Namespace
        uint32_t nsid = nsidx + 1;

        NVME_IDENTIFY_NAMESPACE *identifyNS = malloc(sizeof(NVME_IDENTIFY_NAMESPACE));
        uint64_t                 pagCont    = 1;
        memset(identifyNS, 0, PAGE_SIZE);

        memset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        sqe.CDW0    = NVME_SQE_OPC_ADMIN_IDENTIFY;
        sqe.META    = 0;
        sqe.DATA[0] = (uint64_t)virt_to_phys((uint64_t)identifyNS);
        sqe.DATA[1] = 0;
        sqe.NSID    = nsid;
        sqe.CDWA    = NVME_ADMIN_IDENTIFY_CNS_ID_NS;
        cqe         = NVMEWaitingCMD(&controller->ASQ, &sqe);
        if ((cqe.STS >> 1) & 0xFF) {
            kerror("Nvme cannot identify namespace %08x", nsid);
            return;
        }

        uint8_t currentLBAFormat = identifyNS->FLBA & 0xF;
        if (currentLBAFormat > identifyNS->NLBA) {
            kerror("Nvme namespace %08x current LBA format %08x is beyond what the namespace "
                   "supports %p.",
                   nsid, currentLBAFormat, identifyNS->NLBA + 1, identifyNS);
            return;
        }

        if (!identifyNS->SIZE) { return; }

        if (!NVME_DMA_MEMORY) {
            pagCont         = 1;
            NVME_DMA_MEMORY = malloc(pagCont * PAGE_SIZE);
        }

        NVME_NAMESPACE *ns = (NVME_NAMESPACE *)malloc(sizeof(NVME_NAMESPACE));
        memset(ns, 0, sizeof(NVME_NAMESPACE));
        ns->CTRL = controller;
        ns->NSID = nsid;
        ns->NLBA = identifyNS->SIZE;

        NVME_LOGICAL_BLOCK_ADDRESS *fmt = identifyNS->LBAF + currentLBAFormat;

        ns->BSZ  = 1ULL << fmt->DS;
        ns->META = fmt->MS;
        if (ns->BSZ > PAGE_SIZE) {
            kerror("Nvme namespace %08x block size > 4096.", nsid, ns->BSZ);
            free(ns);
            return;
        }

        if (mdts) {
            ns->MXRS = ((1ULL << mdts) * PAGE_SIZE) / ns->BSZ;
        } else {
            ns->MXRS = -1;
        }

        device_t disk;
        disk.type = DEVICE_BLOCK;
        char name[10];
        sprintf(name, "nvme%d", nsid);
        strcpy(disk.drive_name, name);
        disk.read  = NvmeRead;
        disk.write = NvmeWrite;
        disk.ioctl = (void *)empty;
        disk.poll  = (void *)empty;

        int c          = regist_device(disk);
        nvme_device[c] = ns;
        printk("Nvme disk find (%s)\n", disk.drive_name);
    }
}
