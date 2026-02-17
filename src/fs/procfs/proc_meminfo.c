#include "fs/procfs.h"
#include "mem/frame.h"
#include "mem/memstat.h"
#include "string_builder.h"
#include "term/klog.h"

static bool meminfo_append_kb(string_builder_t *builder, const char *key, uint64_t value_kb) {
    return string_builder_append(builder, "%s:%8llu kB\n", key, value_kb);
}

static bool meminfo_append_raw(string_builder_t *builder, const char *key, uint64_t value) {
    return string_builder_append(builder, "%s:%8llu\n", key, value);
}

char *proc_gen_meminfo(size_t *context_len) {
    string_builder_t *builder = create_string_builder(4096);
    if (unlikely(builder == NULL))
        return NULL;

    const uint64_t mem_total_kb = get_origin_frames() * 4;
    const uint64_t mem_free_kb = get_usable_frames() * 4;
    const uint64_t mem_available_kb = mem_free_kb;
    const uint64_t mem_used_kb = mem_total_kb - mem_free_kb;
    const uint64_t bad_kb = get_bad_memory() / 1024;

    logkf("proc_meminfo: %llu %llu %llu\n\r", mem_total_kb, mem_free_kb, mem_used_kb);

    const uint64_t zero_kb = 0;
    const uint64_t swap_total_kb = 0;
    const uint64_t swap_free_kb = 0;
    const uint64_t commit_limit_kb = mem_total_kb + swap_total_kb;
    const uint64_t committed_as_kb = mem_used_kb;

    if (!meminfo_append_kb(builder, "MemTotal", mem_total_kb))
        goto err;
    if (!meminfo_append_kb(builder, "MemFree", mem_free_kb))
        goto err;
    if (!meminfo_append_kb(builder, "MemAvailable", mem_available_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Buffers", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Cached", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "SwapCached", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Active", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Inactive", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Active(anon)", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Inactive(anon)", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Active(file)", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Inactive(file)", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Unevictable", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Mlocked", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "SwapTotal", swap_total_kb))
        goto err;
    if (!meminfo_append_kb(builder, "SwapFree", swap_free_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Dirty", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Writeback", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "AnonPages", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Mapped", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Shmem", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "KReclaimable", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Slab", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "SReclaimable", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "SUnreclaim", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "KernelStack", STACK_SIZE))
        goto err;
    if (!meminfo_append_kb(builder, "PageTables", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "NFS_Unstable", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Bounce", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "WritebackTmp", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "CommitLimit", commit_limit_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Committed_AS", committed_as_kb))
        goto err;
    if (!meminfo_append_kb(builder, "VmallocTotal", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "VmallocUsed", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "VmallocChunk", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Percpu", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "HardwareCorrupted", bad_kb))
        goto err;
    if (!meminfo_append_kb(builder, "AnonHugePages", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "ShmemHugePages", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "ShmemPmdMapped", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "FileHugePages", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "FilePmdMapped", zero_kb))
        goto err;
    if (!meminfo_append_kb(builder, "Unaccepted", zero_kb))
        goto err;
    if (!meminfo_append_raw(builder, "HugePages_Total", 0))
        goto err;
    if (!meminfo_append_raw(builder, "HugePages_Free", 0))
        goto err;
    if (!meminfo_append_raw(builder, "HugePages_Rsvd", 0))
        goto err;
    if (!meminfo_append_raw(builder, "HugePages_Surp", 0))
        goto err;
    if (!meminfo_append_kb(builder, "Hugepagesize", 0))
        goto err;
    if (!meminfo_append_kb(builder, "Hugetlb", 0))
        goto err;
    if (!meminfo_append_kb(builder, "DirectMap4k", 0))
        goto err;
    if (!meminfo_append_kb(builder, "DirectMap2M", 0))
        goto err;
    if (!meminfo_append_kb(builder, "DirectMap1G", 0))
        goto err;

    *context_len = builder->size;
    char *data = builder->data;
    free(builder);
    return data;
err:
    free(builder->data);
    free(builder);
    return NULL;
}

size_t proc_meminfo_stat(proc_handle_t *handle) {
    size_t content_len = 0;
    char *content = proc_gen_meminfo(&content_len);
    free(content);
    return content_len;
}

size_t proc_meminfo_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t content_len = 0;
    char *content = proc_gen_meminfo(&content_len);

    if (!content || content_len == 0) {
        if (content)
            free(content);
        return 0;
    }

    if (offset >= content_len) {
        free(content);
        return 0;
    }

    content_len = MIN(content_len, offset + size);
    size_t to_copy = MIN(content_len, size);

    memcpy(addr, content + offset, to_copy);
    free(content);

    if (to_copy < size)
        ((char *)addr)[to_copy] = '\0';
    return to_copy;
}
