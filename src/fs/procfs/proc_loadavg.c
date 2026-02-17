#include "fs/procfs.h"
#include "string_builder.h"
#include "task/smp.h"

char *proc_gen_loadavg(size_t *context_len) {
    // 统计运行中的任务数和总任务数
    size_t running = 0;
    size_t total   = 0;
    pid_t last_pid = 0;

    extern cow_arraylist *process_list;
    pcb_t proc = NULL;
    cow_foreach(process_list, proc) {
        total++;
        if (proc->status == T_RUNNING || proc->status == T_START) {
            running++;
        }
        if (proc->pid > last_pid) {
            last_pid = proc->pid;
        }
    }

    // 简化实现: 根据 CPU 利用率估算 load average
    uint64_t total_jiffies = 0;
    uint64_t total_idle    = 0;
    for (size_t i = 0; i < get_cpu_count(); i++) {
        cpu_local_t *info = get_cpu_local(i);
        if (info == NULL || !info->enable) {
            continue;
        }
        total_jiffies += info->jiffies;
        total_idle += info->idle_jiffies;
    }

    // load = (busy / total) * nr_cpus, 用两位小数表示
    uint64_t busy     = total_jiffies - total_idle;
    uint64_t load_100 = 0;
    if (total_jiffies > 0) {
        load_100 = (busy * 100 * get_cpu_count()) / total_jiffies;
    }

    const uint64_t load_int  = load_100 / 100;
    const uint64_t load_frac = load_100 % 100;

    string_builder_t *builder = create_string_builder(64);
    string_builder_append(
        builder,
        "%llu.%02llu %llu.%02llu %llu.%02llu %llu/%llu %d\n",
        load_int,
        load_frac,
        load_int,
        load_frac,
        load_int,
        load_frac,
        (uint64_t)running,
        (uint64_t)total,
        last_pid
    );

    *context_len = builder->size;
    char *data   = builder->data;
    free(builder);
    return data;
}

size_t proc_loadavg_stat(proc_handle_t *handle) {
    size_t length = 0;
    free(proc_gen_loadavg(&length));
    return length;
}

size_t proc_loadavg_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size = 0;
    char *content  = proc_gen_loadavg(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, content);
}
