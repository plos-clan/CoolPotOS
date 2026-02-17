#include "fs/procfs.h"

char *proc_gen_stat_file(pcb_t task, size_t *content_len) {
    char *buffer = malloc(PAGE_SIZE * 4);
    int   len    = sprintf(
        buffer,
        "%d (%s) %c %d %d %d %d %d %u %d %d %d %d %d %d %d %d %d %d "
             "%ld %d %d %lu %d %d %d %d %d %d %d %d %d %d %d %d %d "
             "%d %d %d %u %u %d %d %d %d %d %d %d %d %d %d %d\n",
        task->pid,  // pid
        task->name, // name
        task->status == T_RUNNING  ? 'R'
             : task->status == T_ZOMBIE ? 'Z'
             : task->status == T_FUTEX  ? 'S'
                                        : 'T',              // state
        task->parent->pid,                            // ppid
        0,                                            // pgrp
        task->uid,                                    // session
        0,                                            // tty_nr
        0,                                            // tpgid
        0,                                            // flags
        0,                                            // minflt
        0,                                            // cminflt
        0,                                            // majflt
        0,                                            // cmajflt
        0,                                            // utime
        0,                                            // stime
        0,                                            // cutime
        0,                                            // cstime
        task->pid == 0 ? SCHED_IDLE : SCHED_DEADLINE, // priority
        0,                                            // nicec
        task->child_threads->size,                    // num_threads
        0,                                            // itrealvalue
        0,                                            // starttime
        task->vma_manager.vm_total,                   // vsize
        0,                                            // rss
        0,                                            // rsslim
        0,                                            // startcode
        0,                                            // endcode
        0,                                            // startstack
        0,                                            // kstkesp
        0,                                            // ksteip
        0,                                            // signal
        0,                                            // blocked
        0,                                            // sigignore
        0,                                            // sigcatch
        0,                                            // wchan
        0,                                            // nswap
        0,                                            // cnswap
        0,                                            // exit_signal
        0,                                            // processor
        0,                                            // rt_priority
        0,                                            // policy
        0,                                            // delayacct_blkio_ticks
        0,                                            // guest_time
        0,                                            // cguest_time
        0,                                            // start_data
        0,                                            // end_data
        0,                                            // start_brk
        0,                                            // arg_start
        0,                                            // arg_end
        0,                                            // env_start
        0,                                            // env_end
        0                                             // exit_code
    );

    *content_len = len;

    return buffer;
}

size_t proc_pstat_stat(proc_handle_t *handle) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    size_t content_len = 0;
    char  *content     = proc_gen_stat_file(task, &content_len);
    free(content);
    return content_len;
}

size_t proc_pstat_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->process;
    } else {
        task = handle->task;
    }
    size_t content_len = 0;
    char  *content     = proc_gen_stat_file(task, &content_len);
    if (offset >= content_len) {
        free(content);
        return 0;
    }
    content_len    = MIN(content_len, offset + size);
    size_t to_copy = MIN(content_len, size);
    memcpy(addr, content + offset, to_copy);
    free(content);
    if (to_copy < size)
        ((char *)addr)[to_copy] = '\0';
    return to_copy;
}
