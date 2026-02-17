#include "fs/procfs.h"
#include "intctl.h"
#include "task/smp.h"

char *proc_gen_interrupts(size_t *context_len) {
    extern irq_action_t actions[ARCH_MAX_IRQ_NUM];
    const size_t        bufsize = PAGE_SIZE * 4;
    char               *buffer  = malloc(bufsize);
    if (!buffer)
        return NULL;

    size_t offset = 0;
    memset(buffer, 0, bufsize);

    // 写入 CPU 表头
    offset += snprintf(buffer + offset, bufsize - offset, "           ");
    for (size_t cpu = 0; cpu < get_cpu_count(); cpu++) {
        offset += snprintf(buffer + offset, bufsize - offset, "CPU%-4zu", cpu);
    }
    offset += snprintf(buffer + offset, bufsize - offset, "\n");
    for (size_t irq = 0; irq < ARCH_MAX_IRQ_NUM; irq++) {
        irq_action_t *action = &actions[irq];
        if (action->irq_controller == NULL || action->handler == NULL)
            continue;

        offset += snprintf(buffer + offset, bufsize - offset, "%3zu:    ", irq);

        // 写入每 CPU 的计数
        for (size_t cpu = 0; cpu < get_cpu_count(); cpu++) {
            offset += snprintf(
                buffer + offset, bufsize - offset, "%-8llu",
                (unsigned long long)action->int_count[cpu]
            );
        }

        char *name_type;
        switch (action->type) {
        case IO_APIC:
            name_type = "IO_APIC";
            break;
        case PCI_MSI:
            name_type = "PCI_MSI";
            break;
        }
        offset += snprintf(buffer + offset, bufsize - offset, "%s ", name_type);

        if (action->name)
            offset += snprintf(buffer + offset, bufsize - offset, "%s\n", action->name);
        else
            offset += snprintf(buffer + offset, bufsize - offset, "unknown\n");
    }

    *context_len = offset;
    return buffer;
}

size_t proc_interrupts_stat(proc_handle_t *handle) {
    size_t content_len = 0;
    char  *content     = proc_gen_interrupts(&content_len);
    free(content);
    return content_len;
}

size_t proc_interrupts_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    size_t len     = 0;
    char  *content = proc_gen_interrupts(&len);
    return procfs_node_read(len, offset, size, addr, content);
}
