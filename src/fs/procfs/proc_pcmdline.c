#include "fs/procfs.h"
#include "term/klog.h"

size_t proc_pcmdline_stat(proc_handle_t *handle) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    const size_t length = task->cmdline ? task->cl_length : strlen("no_cmdline");
    return length;
}

size_t proc_pcmdline_read(
     proc_handle_t *handle, void *addr, const size_t offset, const size_t size
) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    const char *cmdline = task->cmdline != NULL ? task->cmdline : "no_cmdline";
    const size_t len    = task->cmdline != NULL ? task->cl_length : strlen(cmdline);

    char *contect = malloc(len);
    memcpy(contect, cmdline, len);

    return procfs_node_read(len, offset, size, addr, contect);
}
