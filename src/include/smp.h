#pragma once

#define MAX_CPU_NUM 256 //CP_Kernel 最大支持识别的CPU核心数

#define APU_BASE_ADDR 0x80000

#define IPI_INIT 0x4500
#define IPI_STARTUP 0x4600

#include "acpi.h"
#include "limine.h"

void apu_startup(struct limine_smp_request smp_request);
uint64_t cpu_num();
