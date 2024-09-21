#ifndef CRASHPOWEROS_FPU_H
#define CRASHPOWEROS_FPU_H

#include "common.h"

typedef struct __attribute__((packed)) fpu_regs {
    uint16_t control;
    uint16_t RESERVED1;
    uint16_t status;
    uint16_t RESERVED2;
    uint16_t tag;
    uint16_t RESERVED3;
    uint32_t fip0;
    uint32_t fop0;
    uint32_t fdp0;
    uint32_t fdp1;
    uint8_t regs[80];
} fpu_regs_t;

void fpu_setup();

#endif
