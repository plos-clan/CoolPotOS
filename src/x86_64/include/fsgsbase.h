#pragma once

#define IA32_FS_BASE        0xc0000100
#define IA32_GS_BASE        0xc0000101
#define IA32_KERNEL_GS_BASE 0xc0000102

#define current_cpu ((smp_cpu_t *)read_kgsbase())

#include "ctype.h"

extern uint64_t (*read_fsbase)();
extern void (*write_fsbase)(uint64_t value);
extern uint64_t (*read_gsbase)();
extern void (*write_gsbase)(uint64_t value);

uint64_t read_fsbase_msr();
void     write_fsbase_msr(uint64_t value);
uint64_t read_gsbase_msr();
void     write_gsbase_msr(uint64_t value);

uint64_t rdfsbase();
void     wrfsbase(uint64_t value);
uint64_t rdgsbase();
void     wrgsbase(uint64_t value);
uint64_t read_kgsbase();
void     write_kgsbase(uint64_t value);

uint32_t has_fsgsbase();
uint64_t fsgsbase_init();
