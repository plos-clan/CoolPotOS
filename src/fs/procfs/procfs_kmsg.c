#include "fs/procfs.h"
#include "term/klog.h"

size_t proc_kmsg_stat(proc_handle_t *handle){
    return kmsg_length();
}

size_t proc_kmsg_read(proc_handle_t *handle,void *addr, size_t offset, size_t size){
    return kmesg_read(addr, size);
}
