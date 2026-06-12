#pragma once

#define IA32_FS_BASE        0xc0000100
#define IA32_GS_BASE        0xc0000101
#define IA32_KERNEL_GS_BASE 0xc0000102

#include "types.h"

uint64_t read_fsbase();
void write_fsbase(uint64_t value);
uint64_t read_kgsbase();
void write_kgsbase(uint64_t value);
uint64_t read_gsbase();
void write_gsbase(uint64_t value);

void fsgsbase_init();
