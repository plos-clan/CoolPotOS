#include "fs/procfs.h"

#include <term/klog.h>

size_t proc_pcmdline_stat(proc_handle_t *handle) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    size_t length = task->cmdline ? task->cl_length : strlen("no_cmdline");
    return length;
}

size_t proc_pcmdline_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    char  *cmdline = task->cmdline != NULL ? task->cmdline : "no_cmdline";
    size_t len     = task->cmdline != NULL ? task->cl_length : strlen(cmdline);
    char  *contect = strdup(cmdline);

    logkf("task(%s:%d): ", task->name, task->cl_length);
    for (size_t i = 0; i < task->cl_length; i++) {
        logkf("%c", task->cmdline[i] == '\0' ? ' ' : task->cmdline[i]);
    }
    logkf("\n");

    return procfs_node_read(len, offset, size, addr, contect);
}
