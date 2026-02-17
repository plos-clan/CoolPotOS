#pragma once

#include "types.h"

struct fdt_serial_device {
    uint64_t base_addr;
    uint32_t irq_num;
    uint32_t reg_shift;
    uint32_t clock_freq;
    uint32_t reg_io_width;
    int found;
};

int init_serial();
