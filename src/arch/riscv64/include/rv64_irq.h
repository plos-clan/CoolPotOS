#pragma once

#include "ptrace.h"

enum page_fault_type {
    LOAD_PAGE,      // 加载读取异常
    STORE_AMO_PAGE, // 写入/原子操作异常
    INS_PAGE,       // 取指异常
};

int trap_init(void);
void page_fault_(struct pt_regs *regs,enum page_fault_type type);
