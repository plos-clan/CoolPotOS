#include "fs/procfs.h"
#include "string_builder.h"

static pcb_t proc_pmaps_task(proc_handle_t *handle) {
    if (handle->task != NULL) {
        return handle->task;
    }
    const tcb_t current = get_current_task();
    return current == NULL ? NULL : current->process;
}

const char *get_vma_permissions(vma_t *vma) {
    static char perms[5];

    perms[0] = (vma->vm_flags & VMA_READ) ? 'r' : '-';
    perms[1] = (vma->vm_flags & VMA_WRITE) ? 'w' : '-';
    perms[2] = (vma->vm_flags & VMA_EXEC) ? 'x' : '-';
    perms[3] = (vma->vm_flags & VMA_SHARED) ? 's' : 'p';
    perms[4] = '\0';

    return perms;
}

char *proc_gen_maps_file(pcb_t task, size_t *content_len) {
    if (content_len == NULL) {
        return NULL;
    }
    if (task == NULL) {
        *content_len = 0;
        return strdup("");
    }

    vma_t *vma                = task->mm->vma_manager.vma_list;
    fdt_t *fdt                = task->fdts;
    string_builder_t *builder = create_string_builder(4096);
    if (builder == NULL) {
        *content_len = 0;
        return strdup("");
    }

    while (vma) {
        vfs_node_t node = NULL;
        if (vma->vm_fd != -1 && fdt != NULL) {
            const fd_t *fd_handle = get_fd(fdt, vma->vm_fd);
            if (fd_handle != NULL) {
                node = fd_handle->node;
            }
        }

        string_builder_append(
            builder,
            "%012lx-%012lx %s %08lx %02x:%02x %lu",
            vma->vm_start,
            vma->vm_end,
            get_vma_permissions(vma),
            vma->vm_offset,
            node ? (node->rdev >> 8) & 0xFF : 0,
            node ? node->rdev & 0xFF : 0,
            node ? node->inode : 0
        );

        const char *pathname = vma->vm_name;
        if (pathname && strlen(pathname) > 0) {
            string_builder_append(builder, "%*s%s", 15, "", pathname);
        }

        string_builder_append(builder, "\n");

        vma = vma->vm_next;
    }

    *content_len = builder->size;

    void *data = builder->data;

    free(builder);

    return data;
}

size_t proc_pmaps_stat(proc_handle_t *handle) {
    pcb_t task         = proc_pmaps_task(handle);
    size_t content_len = 0;
    char *content      = proc_gen_maps_file(task, &content_len);
    free(content);
    return content_len;
}

size_t proc_pmaps_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    pcb_t task         = proc_pmaps_task(handle);
    size_t content_len = 0;
    char *content      = proc_gen_maps_file(task, &content_len);
    if (offset >= content_len) {
        free(content);
        return 0;
    }
    size_t to_copy = MIN(content_len - offset, size);
    memcpy(addr, content + offset, to_copy);
    free(content);
    if (to_copy < size)
        ((char *)addr)[to_copy] = '\0';
    return to_copy;
}
