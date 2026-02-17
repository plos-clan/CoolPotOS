#include "fs/procfs.h"
#include "string_builder.h"
#include "timer.h"
#include "task/smp.h"

char *proc_gen_uptime(size_t *context_len) {
    uint64_t ns          = nano_time();
    uint64_t uptime_sec  = ns / 1000000000ULL;
    uint64_t uptime_frac = (ns % 1000000000ULL) / 10000000ULL; // 两位小数

    // idle 时间: 所有 CPU 的 idle_jiffies 之和, 转换为秒 (100Hz)
    uint64_t total_idle_jiffies = 0;
    for (size_t i = 0; i < get_cpu_count(); i++) {
        cpu_local_t *info = get_cpu_local(i);
        if (info == NULL || !info->enable)
            continue;
        total_idle_jiffies += info->idle_jiffies;
    }
    uint64_t idle_sec  = total_idle_jiffies / 100;
    uint64_t idle_frac = total_idle_jiffies % 100;

    string_builder_t *builder = create_string_builder(64);
    string_builder_append(
        builder, "%llu.%02llu %llu.%02llu\n", uptime_sec, uptime_frac, idle_sec, idle_frac
    );

    *context_len = builder->size;
    char *data   = builder->data;
    free(builder);
    return data;
}

size_t proc_uptime_stat(proc_handle_t *handle) {
    size_t length = 0;
    free(proc_gen_uptime(&length));
    return length;
}

size_t proc_uptime_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size;
    char  *content = proc_gen_uptime(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, content);
}
