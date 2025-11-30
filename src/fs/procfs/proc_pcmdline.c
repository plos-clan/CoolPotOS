#include "fs/procfs.h"

size_t proc_pcmdline_stat(proc_handle_t *handle){
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    char  *cmdline = task->cmdline ? task->cmdline : "no_cmdline";
    return strlen(cmdline);
}

size_t proc_pcmdline_read(proc_handle_t *handle,void *addr, size_t offset, size_t size){
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    char  *cmdline = task->cmdline ? task->cmdline : "no_cmdline";
    size_t len     = strlen(cmdline);
    char  *contect = strdup(cmdline);
    return procfs_node_read(len, offset, size, addr, contect);
}
