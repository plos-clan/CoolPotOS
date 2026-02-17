#include "fs/procfs.h"
#include "intctl.h"
#include "metadata.h"
#include "string_builder.h"
#include "task/smp.h"
#include "timer.h"

char *proc_gen_stat(size_t *context_len) {
    string_builder_t *builder = create_string_builder(4096);
    size_t cpu_count = get_cpu_count();

    uint64_t total_idle = 0;
    uint64_t total_system = 0;
    size_t processes_all = 0;
    size_t procs_running = 0;

    for (size_t i = 0; i < cpu_count; i++) {
        cpu_local_t *info = get_cpu_local(i);
        if (info == NULL || !info->enable)
            continue;
        total_system += info->jiffies - info->idle_jiffies;
        total_idle += info->idle_jiffies;
        processes_all += info->task_count;
        if (info->current_task && info->current_task->status == T_RUNNING)
            procs_running++;
    }

    string_builder_append(builder, "cpu  0 0 %llu %llu 0 0 0 0 0 0\n", total_system, total_idle);

    for (size_t i = 0; i < cpu_count; i++) {
        cpu_local_t *info = get_cpu_local(i);
        if (info == NULL || !info->enable)
            continue;
        uint64_t sys = info->jiffies - info->idle_jiffies;
        uint64_t idle = info->idle_jiffies;
        string_builder_append(
            builder, "cpu%llu 0 0 %llu %llu 0 0 0 0 0 0\n", (uint64_t)i, sys, idle);
    }

    string_builder_append(builder, "intr %llu\n", get_all_irq_count());
    string_builder_append(builder, "ctxt %llu\n", total_system + total_idle);

    int64_t btime = mktime_universal();
    if (btime > 0) {
        uint64_t uptime_sec = nano_time() / 1000000000ULL;
        string_builder_append(builder, "btime %llu\n", (uint64_t)(btime - uptime_sec));
    } else {
        string_builder_append(builder, "btime 0\n");
    }

    string_builder_append(builder, "processes %llu\n", (uint64_t)processes_all);
    string_builder_append(builder, "procs_running %llu\n", (uint64_t)procs_running);
    string_builder_append(builder, "procs_blocked 0\n");
    string_builder_append(builder, "softirq 0 0 0 0 0 0 0 0 0 0 0\n");

    *context_len = builder->size;
    char *data = builder->data;
    free(builder);
    return data;
}

size_t proc_stat_stat(proc_handle_t *handle) {
    size_t length = 0;
    free(proc_gen_stat(&length));
    return length;
}

size_t proc_stat_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size;
    char *contect = proc_gen_stat(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, contect);
}
