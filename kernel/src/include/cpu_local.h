#pragma once

#include "mem/page.h"
#include "task/task.h"

typedef struct cpu_local_info {
    page_directory_t current_dir;
    tcb_t current_task;
    size_t cpu_id;
    void *arch_local_info;
} cpu_local_t;

cpu_local_t *get_current_cpu();

void arch_bsp_cpu_setup();
