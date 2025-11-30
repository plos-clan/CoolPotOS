#include "fs/procfs.h"
#include "mem/frame.h"
#include "mem/memstat.h"

char *proc_gen_meminfo(size_t *context_len){
    char *meminfo_origin =
        "MemTotal:\t\t%llu kB\n"
        "MemFree:\t\t%llu kB\n"
        "MemAvailable:\t%llu kB\n"
        "Buffers:\t\t%llu kB\n"
        "Cached:\t\t%llu kB\n"
        "SwapCached:\t\t%llu kB\n"
        "Active:\t\t%llu kB\n"
        "Inactive:\t\t%llu kB\n"
        "Active(anon):\t%llu kB\n"
        "Inactive(anon):\t%llu kB\n"
        "Active(file):\t%llu kB\n"
        "Inactive(file):\t%llu kB\n"
        "Unevictable:\t\t%llu kB\n"
        "Mlocked:\t\t%llu kB\n"
        "SwapTotal:\t\t%llu kB\n"
        "SwapFree:\t\t%llu kB\n"
        "Zswap:\t\t%llu kB\n"
        "Zswapped:\t\t%llu kB\n"
        "Dirty:\t\t%llu kB\n"
        "Writeback:\t\t%llu kB\n"
        "AnonPages: \t\t%llu kB\n"
        "Mapped:\t\t%llu kB\n"
        "Shmem:\t\t%llu kB\n"
        "KReclaimable:\t%llu kB\n"
        "Slab:\t\t%llu kB\n"
        "SReclaimable:\t\t%llu kB\n"
        "SUnreclaim:\t\t%llu kB\n"
        "KernelStack:\t\t%llu kB\n"
        "PageTables:\t\t%llu kB\n"
        "SecPageTables:\t\t%llu kB\n"
        "NFS_Unstable:\t\t%llu kB\n"
        "Bounce:\t\t%llu kB\n"
        "WritebackTmp:\t\t%llu kB\n"
        "CommitLimit:\t%llu kB\n"
        "Committed_AS:\t%llu kB\n"
        "VmallocTotal:\t%llu kB\n"
        "VmallocUsed:\t\t%llu kB\n"
        "VmallocChunk:\t\t%llu kB\n"
        "Percpu:\t\t%llu kB\n"
        "HardwareCorrupted:\t%llu kB\n"
        "AnonHugePages:\t%llu kB\n"
        "ShmemHugePages:\t\t%llu kB\n"
        "ShmemPmdMapped:\t\t%llu kB\n"
        "FileHugePages:\t\t%llu kB\n"
        "FilePmdMapped:\t\t%llu kB\n"
        "Unaccepted:\t\t%llu kB\n"
        "HugePages_Total:\t\t%llu\n"
        "HugePages_Free:\t\t%llu\n"
        "HugePages_Rsvd:\t\t%llu\n"
        "HugePages_Surp:\t\t%llu\n"
        "Hugepagesize:\t%llu kB\n"
        "Hugetlb:\t\t%llu kB\n"
        "DirectMap4k:\t%llu kB\n"
        "DirectMap2M:\t%llu kB\n"
        "DirectMap1G:\t%llu kB\n";

#define MEMINFO_TODO 0      // Things to do in the future ;)

    size_t estimated_size = strlen(meminfo_origin) * 4;
    char *result = malloc(estimated_size);
    if (!result) {
        *context_len = 0;
        return NULL;
    }

    int len = snprintf(result, estimated_size, meminfo_origin,
                       get_memory_size()                       / 1024, // MemTotal
                       (get_memory_size() - get_used_memory()) / 1024, // MemFree
                       get_available_memory()                  / 1024, // MemAvailable
                       MEMINFO_TODO,   // Buffers
                       MEMINFO_TODO,   // Cached
                       MEMINFO_TODO,   // SwapCached
                       MEMINFO_TODO,   // Active
                       MEMINFO_TODO,   // Inactive
                       MEMINFO_TODO,   // Active(anon)
                       MEMINFO_TODO,   // Inactive(anon)
                       MEMINFO_TODO,   // Active(file)
                       MEMINFO_TODO,   // Inactive(file)
                       MEMINFO_TODO,   // Unevictable
                       MEMINFO_TODO,   // Mlocked
                       MEMINFO_TODO,   // SwapTotal
                       MEMINFO_TODO,   // SwapFree
                       MEMINFO_TODO,   // Zswap
                       MEMINFO_TODO,   // Zswapped
                       MEMINFO_TODO,   // Dirty
                       MEMINFO_TODO,   // Writeback
                       MEMINFO_TODO,   // AnonPages
                       MEMINFO_TODO,   // Mapped
                       MEMINFO_TODO,   // Shmem
                       MEMINFO_TODO,   // KReclaimable
                       MEMINFO_TODO,   // Slab
                       MEMINFO_TODO,   // SReclaimable
                       MEMINFO_TODO,   // SUnreclaim
                       MEMINFO_TODO,   // KernelStack
                       MEMINFO_TODO,   // PageTables
                       MEMINFO_TODO,   // SecPageTables
                       MEMINFO_TODO,   // NFS_Unstable
                       MEMINFO_TODO,   // Bounce
                       MEMINFO_TODO,   // WritebackTmp
                       MEMINFO_TODO,   // CommitLimit
                       MEMINFO_TODO,   // Committed_AS
                       MEMINFO_TODO,   // VmallocTotal
                       MEMINFO_TODO,   // VmallocUsed
                       MEMINFO_TODO,   // VmallocChunk
                       MEMINFO_TODO,   // Percpu
                       MEMINFO_TODO,   // HardwareCorrupted
                       MEMINFO_TODO,   // AnonHugePages
                       MEMINFO_TODO,   // ShmemHugePages
                       MEMINFO_TODO,   // ShmemPmdMapped
                       MEMINFO_TODO,   // FileHugePages
                       MEMINFO_TODO,   // FilePmdMapped
                       MEMINFO_TODO,   // Unaccepted
                       MEMINFO_TODO,   // HugePages_Total
                       MEMINFO_TODO,   // HugePages_Free
                       MEMINFO_TODO,   // HugePages_Rsvd
                       MEMINFO_TODO,   // HugePages_Surp
                       MEMINFO_TODO,   // Hugepagesize
                       MEMINFO_TODO,   // Hugetlb
                       MEMINFO_TODO,   // DirectMap4k
                       MEMINFO_TODO,   // DirectMap2M
                       MEMINFO_TODO   // DirectMap1G
    );

    if (len < 0 || len >= estimated_size) {
        free(result);
        *context_len = 0;
        return NULL;
    }

    *context_len = len;
    return result;
}

size_t proc_meminfo_stat(proc_handle_t *handle){
    size_t content_len = 0;
    char *content = proc_gen_meminfo(&content_len);
    free(content);
    return content_len;
}

size_t proc_meminfo_read(proc_handle_t *handle,void *addr, size_t offset, size_t size){
    size_t content_len = 0;
    char *content = proc_gen_meminfo(&content_len);

    if (!content || content_len == 0) {
        if (content) free(content);
        return 0;
    }

    if (offset >= content_len) {
        free(content);
        return 0;
    }

    content_len    = MIN(content_len, offset + size);
    size_t to_copy = MIN(content_len, size);

    memcpy(addr, content + offset, to_copy);
    free(content);

    ((char *)addr)[to_copy] = '\0';
    return to_copy;
}
