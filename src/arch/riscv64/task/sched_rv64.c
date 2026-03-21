#include "exec/elf_load.h"
#include "fs/vfs.h"
#include "io.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/smp.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

extern void kernel_thread_func();                        // kthread.S
extern void fpu_save_context(fpu_context_t *fpu_ctx);    // fpu_context.S
extern void fpu_restore_context(fpu_context_t *fpu_ctx); // fpu_context.S
extern void ret_from_trap_handler();                     // vector.s

size_t sched_clock() {
    return nano_time();
}

void arch_send_scheduler() {
    // TODO
}

void arch_context_free(tcb_t thread) {
    // TODO
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    const uint64_t stack_top = (uint64_t)new_task + STACK_SIZE;
    memset(&new_task->context, 0, sizeof(struct arch_context_));
    memset(&new_task->context.fpu_ctx, 0, sizeof(fpu_context_t));
    new_task->context.fpu_ctx.fcsr = FCSR_INIT_DEFAULT;
    new_task->context.ctx          = (struct pt_regs *)stack_top - 1;
    new_task->context.ctx->sstatus =
        2UL << 32 | 1UL << 18 | 1UL << 5 | 1UL << 0 | 1UL << 8 | 1UL << 1;
    new_task->context.ctx->s1  = new_task->_start;
    new_task->context.ctx->a2  = (uint64_t)args;
    new_task->context.ra       = (uint64_t)kernel_thread_func;
    new_task->context.ctx->epc = (uint64_t)kernel_thread_func;
    new_task->context.ctx->sp  = (uint64_t)new_task->context.ctx;
    new_task->context.sp       = (uint64_t)new_task->context.ctx;
    new_task->context.dead     = false;
    new_task->context.ctx->tp  = (uint64_t)arch_current_cpu();
    new_task->context.ctx->ktp = (uint64_t)arch_current_cpu();
}

void arch_context_init(tcb_t thread, struct arch_context_ *context) {
    const uint64_t stack_top = (uint64_t)thread + STACK_SIZE;
    thread->context.ctx      = (struct pt_regs *)stack_top - 1;
    context->ctx->sstatus =
        2UL << 32 | 1UL << 18 | 3UL << 13 | 1UL << 5 | 1UL << 0 | 1UL << 8 | 1UL << 1;
    context->sp       = (uint64_t)context->ctx;
    context->ctx->s1  = 0;
    context->ctx->a2  = 0;
    context->ctx->sp  = (uint64_t)context->ctx;
    context->ctx->tp  = (uint64_t)arch_current_cpu();
    context->ctx->ktp = (uint64_t)arch_current_cpu();
}

USED void __switch_to(tcb_t current, tcb_t next) {
    const page_directory_t *dir = current->process->directory;
    if (dir != next->process->directory) {
        switch_page_directory(next->process->directory);
    }

    if (current->context.ctx->sstatus & 1UL << 63) {
        if (SSTATUS_GET_FS(current->context.ctx->sstatus) == 3) {
            fpu_save_context(&current->context.fpu_ctx);
            SSTATUS_SET_FS(current->context.ctx->sstatus, 2);
        }
    }

    if (SSTATUS_GET_FS(next->context.ctx->sstatus) != 0) {
        fpu_restore_context(&next->context.fpu_ctx);
        SSTATUS_SET_FS(next->context.ctx->sstatus, 2);
    }
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    switch_to(current, next);
}

static uint64_t push_slice(uint64_t ustack, uint8_t *slice, uint64_t len) {
    uint64_t tmp_stack = ustack;
    tmp_stack -= len;
    tmp_stack -= tmp_stack % 0x10;
    memcpy((void *)tmp_stack, slice, len);
    return tmp_stack;
}

static uint64_t build_user_stack(
    tcb_t task,
    uint64_t sp,
    uint64_t entry_point,
    uint64_t link_start,
    uint8_t *link_data,
    size_t link_size,
    uint64_t link_phys,
    size_t link_pages,
    uint8_t *src_data,
    uint64_t load_start
) {
    uint64_t env_i  = 0;
    uint64_t argv_i = 0;

    int argc    = 0;
    char **argv = restore_argv(task->process->cmdline, task->process->cl_length, &argc);
    char **envp = task->process->envp;

    uint64_t tmp_stack = sp;
    tmp_stack          = push_slice(tmp_stack, (uint8_t *)task->name, strlen(task->name) + 1);
    uint64_t execfn_ptr = tmp_stack;

    uint64_t *envps = malloc(1024);
    uint64_t *argvps = malloc(1024);
    memset(envps, 0, 1024);
    memset(argvps, 0, 1024);

    if (envp != NULL) {
        for (env_i = 0; env_i < task->process->envc; env_i++) {
            tmp_stack    = push_slice(tmp_stack, (uint8_t *)envp[env_i], strlen(envp[env_i]) + 1);
            envps[env_i] = tmp_stack;
        }
    }

    for (argv_i = 0; argv_i < (uint64_t)argc; argv_i++) {
        tmp_stack      = push_slice(tmp_stack, (uint8_t *)argv[argv_i], strlen(argv[argv_i]) + 1);
        argvps[argv_i] = tmp_stack;
    }

    size_t total_length = 2 * sizeof(uint64_t) + 7 * 2 * sizeof(uint64_t)
                          + env_i * sizeof(uint64_t) + sizeof(uint64_t)
                          + argv_i * sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
    tmp_stack -= (tmp_stack - total_length) % 0x10;

    uint8_t *tmp = malloc(2 * sizeof(uint64_t));
    memset(tmp, 0, 2 * sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    page_map_range_to_random(
        task->process->directory,
        EHDR_START_ADDR,
        task->process->exec->size,
        ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_WRITE | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_USER
    );
    memcpy((void *)EHDR_START_ADDR, src_data, task->process->exec->size);

    if (link_data != NULL) {
        page_map_range_to_random(
            task->process->directory,
            INTERPRETER_EHDR_ADDR,
            link_size,
            ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_WRITE | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_USER
        );
        memcpy((void *)INTERPRETER_EHDR_ADDR, link_data, link_size);
    }

    Elf64_Ehdr *ehdr  = (Elf64_Ehdr *)EHDR_START_ADDR;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(ehdr->e_phoff + load_start);

    ((uint64_t *)tmp)[0] = AT_NULL;
    ((uint64_t *)tmp)[1] = 0;

    ((uint64_t *)tmp)[0] = AT_PHDR;
    ((uint64_t *)tmp)[1] = (uint64_t)phdrs;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_PHENT;
    ((uint64_t *)tmp)[1] = sizeof(Elf64_Phdr);
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_PHNUM;
    ((uint64_t *)tmp)[1] = ehdr->e_phnum;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_ENTRY;
    ((uint64_t *)tmp)[1] = entry_point;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_UID;
    ((uint64_t *)tmp)[1] = 0;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_EUID;
    ((uint64_t *)tmp)[1] = 0;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_GID;
    ((uint64_t *)tmp)[1] = 0;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_EGID;
    ((uint64_t *)tmp)[1] = 0;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_SECURE;
    ((uint64_t *)tmp)[1] = 0;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    uint8_t random_bytes[16];
    for (int i = 0; i < 16; i++) {
        random_bytes[i] = (uint8_t)(i * 17 + 42);
    }
    tmp_stack            = push_slice(tmp_stack, random_bytes, 16);
    uint64_t random_addr = tmp_stack;

    ((uint64_t *)tmp)[0] = AT_RANDOM;
    ((uint64_t *)tmp)[1] = random_addr;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_EXECFN;
    ((uint64_t *)tmp)[1] = execfn_ptr;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    if (link_start != 0) {
        ((uint64_t *)tmp)[0] = AT_BASE;
        ((uint64_t *)tmp)[1] = link_start;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));
    }

    ((uint64_t *)tmp)[0] = AT_PAGESZ;
    ((uint64_t *)tmp)[1] = PAGE_SIZE;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    memset(tmp, 0, 2 * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, (uint8_t *)envps, env_i * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, (uint8_t *)argvps, argv_i * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, (uint8_t *)&argv_i, sizeof(uint64_t));

    free(tmp);
    free(envps);
    free(argvps);
    if (link_phys != 0) {
        free_frames(link_phys, link_pages);
    } else {
        free(link_data);
    }
    free_argv(argv);

    return tmp_stack;
}

_Noreturn void arch_switch_to_user_mode() {
    tcb_t current = get_current_task();
    pcb_t process = current->process;

    if (process->exec == NULL) {
        logkf("process exec file handle is null.\n");
        goto err;
    }

    uint8_t *data = malloc(process->exec->size);
    if (vfs_read(process->exec, data, 0, process->exec->size) == (size_t)-1) {
        logkf("process exec read file failed.\n");
        free(data);
        goto err;
    }

    uint64_t load_start = 0;
    void *entry         = load_executor_elf(data, process->directory, 0, &load_start, process);
    if (entry == NULL) {
        logkf("cannot load process exec file.\n");
        free(data);
        goto err;
    }

    current->context.user_stack = page_alloc_random(
        process->directory,
        BIG_USER_STACK + PAGE_SIZE,
        ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_WRITE | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_USER
    );
    current->context.user_stack_top = current->context.user_stack + BIG_USER_STACK;
    uint64_t user_sp                = current->context.user_stack_top;

    vma_t *stack_vma = vma_alloc();
    stack_vma->vm_start = current->context.user_stack;
    stack_vma->vm_end   = current->context.user_stack_top;
    stack_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;
    stack_vma->vm_type = VMA_TYPE_ANON;
    stack_vma->vm_name = strdup("[stack]");

    vma_t *region = vma_find_intersection(
        &process->vma_manager,
        current->context.user_stack,
        current->context.user_stack_top
    );
    if (!region) {
        vma_insert(&process->vma_manager, stack_vma);
    }

    if (is_dynamic((Elf64_Ehdr *)data)) {
        uint64_t linker_start = UINT64_MAX;
        void *linker_main     = NULL;
        uint8_t *link_data    = NULL;
        size_t link_size      = 0;
        uint64_t link_phys    = 0;
        size_t link_pages     = 0;

        linker_main = load_interpreter_elf(
            data,
            get_current_directory(),
            &linker_start,
            &link_data,
            &link_size,
            &link_phys,
            &link_pages
        );
        if (linker_main == NULL) {
            logkf("elf_load: Cannot load libc module.\n");
            free(data);
            goto err;
        }

        linker_main = (void *)((uintptr_t)linker_main + linker_start);

        vma_t *ld_so_vma = vma_alloc();
        ld_so_vma->vm_start = linker_start;
        ld_so_vma->vm_end   = linker_start + link_size;
        ld_so_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;
        ld_so_vma->vm_type = VMA_TYPE_ANON;
        ld_so_vma->vm_name = strdup("[libc]");

        region = vma_find_intersection(&process->vma_manager, linker_start, linker_start + link_size);
        if (!region) {
            vma_insert(&process->vma_manager, ld_so_vma);
        }

        user_sp = build_user_stack(
            current,
            user_sp,
            (uint64_t)entry,
            linker_start,
            link_data,
            link_size,
            link_phys,
            link_pages,
            data,
            load_start
        );
        entry = linker_main;
    } else {
        user_sp =
            build_user_stack(current, user_sp, (uint64_t)entry, 0, NULL, 0, 0, 0, data, load_start);
    }
    free(data);

    struct arch_context_ *context = &current->context;
    const uint64_t stack_top      = (uint64_t)current + STACK_SIZE;
    context->ctx                  = (struct pt_regs *)stack_top - 1;
    context->ra                   = (uint64_t)ret_from_trap_handler;
    context->sp                   = (uint64_t)context->ctx;
    context->ctx->ktp             = (uint64_t)arch_current_cpu();
    context->ctx->tp              = (uint64_t)arch_current_cpu();
    context->ctx->epc             = (uint64_t)entry;
    context->ctx->sp              = user_sp;
    context->ctx->sstatus         = 2UL << 32 | 1UL << 18 | 3UL << 13 | 1UL << 5 | 1UL << 0;

    arch_close_interrupt();
    __asm__ volatile("mv sp, %0\n\t"
                     "j ret_from_trap_handler\n\t" : : "r"(context->ctx));

err:
    arch_open_interrupt();
    if (process->child_threads && process->child_threads->size <= 1) {
        kill_proc(process, -1, true);
    } else {
        kill_thread(current);
    }
    while (true) {
        arch_wait_for_interrupt();
    }
}

bool arch_check_user_mode(const struct pt_regs *regs) {
    return true; //TODO
}
