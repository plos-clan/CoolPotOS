#pragma once

#define MAX_PID_NAME_LEN 21

#define SCHED_NORMAL 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_IDLE 5
#define SCHED_DEADLINE 6

#include "fs/vfs.h"
#include "lock.h"
#include "task/task.h"

typedef struct proc_handle proc_handle_t;

typedef size_t (*stat_entry_t)(proc_handle_t *handle);
typedef size_t (*read_entry_t)(proc_handle_t *handle, void *addr, size_t offset, size_t size);

struct proc_handle {
    char name[64];
    char content[256];
    vfs_node_t node;
    pcb_t task;
};

typedef struct proc_handle_node {
    char *name;
    uint64_t hash;
    read_entry_t read_entry;
    stat_entry_t stat_entry;
} proc_handle_node_t;

typedef struct procfs_self_handle {
    vfs_node_t self;
} procfs_self_handle_t;

void load_procfs_root();
void procfs_stat_dispatch(proc_handle_t *handle, vfs_node_t node);
size_t procfs_read_dispatch(proc_handle_t *handle, void *addr, size_t offset, size_t size);

size_t proc_filesystems_stat(proc_handle_t *handle);
size_t proc_filesystems_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_cmdline_stat(proc_handle_t *handle);
size_t proc_cmdline_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_mounts_stat(proc_handle_t *handle);
size_t proc_mounts_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_interrupts_stat(proc_handle_t *handle);
size_t proc_interrupts_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_kmsg_stat(proc_handle_t *handle);
size_t proc_kmsg_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_pcmdline_stat(proc_handle_t *handle);
size_t proc_pcmdline_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_pmaps_stat(proc_handle_t *handle);
size_t proc_pmaps_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_pstat_stat(proc_handle_t *handle);
size_t proc_pstat_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_meminfo_stat(proc_handle_t *handle);
size_t proc_meminfo_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_cpuinfo_stat(proc_handle_t *handle);
size_t proc_cpuinfo_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_stat_stat(proc_handle_t *handle);
size_t proc_stat_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_uptime_stat(proc_handle_t *handle);
size_t proc_uptime_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);
size_t proc_loadavg_stat(proc_handle_t *handle);
size_t proc_loadavg_read(proc_handle_t *handle, void *addr, size_t offset, size_t size);

size_t procfs_node_read(size_t len, size_t offset, size_t size, char *addr, char *contect);

void procfs_update_task_list();
void procfs_on_new_task(pcb_t task);
void procfs_on_exit_task(pcb_t task);
void procfs_setup();
