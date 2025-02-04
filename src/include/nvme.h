#pragma once

#define NVME_CAP_OFFSET 0x0000
#define NVME_VERSION_OFFSET 0x0008
#define NVME_CC_OFFSET 0x0014
#define NVME_CSTS_OFFSET 0x001C

#include "ctype.h"

typedef struct nvme_registers {
    uint64_t cap;
    uint32_t vs;
    uint32_t int_ms;
    uint32_t int_mc;
    uint32_t cc;
    uint32_t rsvd1;
    uint32_t csts;
    uint32_t rsvd2;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
}__attribute__((packed)) NvmeRegisters;

void nvme_setup();
