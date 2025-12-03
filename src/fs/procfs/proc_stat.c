#include "fs/procfs.h"
#include "intctl.h"
#include "task/smp.h"
#include "string_builder.h"

static bool gen_ctxt_processes(string_builder_t *builder){
    size_t all_ctxt = 0;
    size_t processes_all = 0;
    for (size_t i = 0; i < get_cpu_count(); i++) {
        cpu_local_t *info = get_cpu_local(i);
        if(info == NULL) continue;
        all_ctxt += info->jiffies;
        processes_all += info->task_count;
    }
    bool status = string_builder_append(builder,"ctxt %llu\n",all_ctxt);
    status &= string_builder_append(builder,"processes %llu\n",processes_all);
    return !status;
}

char *proc_gen_stat(size_t *context_len) {
    string_builder_t *builder = create_string_builder(4096);

    if(gen_ctxt_processes(builder)) goto end;

end:
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
    char  *contect = proc_gen_stat(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, contect);
}
