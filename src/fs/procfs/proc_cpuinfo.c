#include "cpu_features.h"
#include "fs/procfs.h"
#include "task/smp.h"

bool gen_processor(string_builder_t *builder, cpu_local_t *info) {
    if (info == NULL)
        return true;
    bool status = true;

    cpu_features_t *features = get_global_features();

    char *flags = calloc(features->features->size + 1, sizeof(char));
    memcpy(flags, features->features->data, features->features->size);

    status &= string_builder_append(builder, "processor       : %d\n", info->id);
    status &= string_builder_append(builder, "vendor_id       : %s\n", features->vendor_id);
    status &= string_builder_append(builder, "model name      : %s\n", features->model_name);
    status &= string_builder_append(builder, "flags           : %s\n", flags);
    status &= string_builder_append(
        builder, "address sizes   : %d bits physical, %d bits virtual\n", features->phys_bits,
        features->virt_bits
    );

    free(flags);
    return status;
}

char *proc_gen_cpuinfo(size_t *context_len) {
    string_builder_t *builder = create_string_builder(4096);
    bool              status  = true;
    for (size_t i = 0; i < get_cpu_count(); i++) {
        status &= gen_processor(builder, get_cpu_local(i));
        if (!status)
            break;
    }
    *context_len = builder->size;
    char *data   = builder->data;
    free(builder);
    return data;
}

size_t proc_cpuinfo_stat(proc_handle_t *handle) {
    size_t length = 0;
    free(proc_gen_cpuinfo(&length));
    return length;
}

size_t proc_cpuinfo_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t fs_size;
    char  *contect = proc_gen_cpuinfo(&fs_size);
    return procfs_node_read(fs_size, offset, size, addr, contect);
}
