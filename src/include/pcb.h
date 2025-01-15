#pragma once

#define TASK_KERNEL_LEVEL 0
#define TASK_SYSTEM_SERVICE_LEVEL 1
#define TASK_APPLICATION_LEVEL 2

#include "ctype.h"
#include "isr.h"

struct process_control_block{
    uint8_t task_level;           // 进程等级< 0:内核 | 1:系统服务 | 2:应用程序 >
    int pid;                      // 进程 PID
    uint64_t cpu_clock;           // CPU 调度时间片
    interrupt_frame_t context;    // 进程上下文

};

typedef struct process_control_block *pcb_t;

void init_pcb();
