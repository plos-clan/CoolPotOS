#pragma once

#define MAX_PID_NAME_LEN 5

#include "pcb.h"
#include "vfs.h"

typedef struct proc_handle {
    char       name[64];
    char       content[256];
    vfs_node_t node;
    pcb_t      task;
} proc_handle_t;

typedef struct procfs_self_handle {
    vfs_node_t self;
} procfs_self_handle_t;

void procfs_update_task_list();
void procfs_on_new_task(pcb_t task);
void procfs_on_exit_task(pcb_t task);
void procfs_setup();
