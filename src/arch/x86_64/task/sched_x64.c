#include "exec/elf_load.h"
#include "fs/vfs.h"
#include "fsgsbase.h"
#include "hpet.h"
#include "intctl.h"
#include "io.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "ptrace.h"
#include "task/smp.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

spin_t tsc_lock = SPIN_INIT;

__attribute__((naked, noreturn)) void
arch_run_on_kernel_stack(uint64_t stack_top, arch_stack_entry_t entry, void *arg) {
    __asm__ volatile("mov %rdi, %rsp\n\t"
                     "andq $-16, %rsp\n\t"
                     "xorq %rbp, %rbp\n\t"
                     "mov %rdx, %rdi\n\t"
                     "call *%rsi\n\t"
                     "ud2\n\t");
}

void cpuid(uint32_t code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code) : "memory");
}

bool cpuid_has_sse() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return edx & (1 << 25);
}

bool cpu_has_rdtsc() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 4)) != 0;
}

uint64_t read_tsc() {
    uint64_t rax, rdx;
    asm volatile("rdtscp\n" : "=a"(rax), "=d"(rdx) : : "ecx");
    return (rdx << 32) + rax;
}

size_t sched_clock() {
    return nano_time();
}

void arch_send_scheduler() {
    __asm__ volatile("int %0\n\r" ::"i"(timer));
}

void calibrate_tsc_with_hpet() {
    spin_lock(tsc_lock);
    bool is_bsp                               = arch_current_cpu()->id == get_bsp_cpu_id();
    arch_current_cpu()->arch_data.support_tsc = cpu_has_rdtsc();
    if (!arch_current_cpu()->arch_data.support_tsc)
        goto end;
    const uint64_t target_ns = 10 * 1000 * 1000;
    uint64_t tsc_start       = read_tsc();
    uint64_t ns_start        = nano_time();
    nsleep(target_ns);
    uint64_t tsc_end   = read_tsc();
    uint64_t ns_end    = nano_time();
    uint64_t delta_tsc = tsc_end - tsc_start;
    uint64_t delta_ns  = ns_end - ns_start;
    if (delta_tsc == 0 || delta_ns == 0)
        return;
    arch_current_cpu()->arch_data.tsc_conv_shift = 22;
    arch_current_cpu()->arch_data.tsc_conv_mul =
        (uint32_t)((delta_ns << arch_current_cpu()->arch_data.tsc_conv_shift) / delta_tsc);
    arch_current_cpu()->arch_data.tsc_base_tsc = read_tsc();
    arch_current_cpu()->arch_data.tsc_base_ns  = nano_time();
    uint64_t freq_hz                           = (delta_tsc * 1000000000ull) / delta_ns;
    if (is_bsp)
        kinfo("Estimated TSC frequency: %llu MHz", freq_hz / 1000 / 1000);
end:
    if (is_bsp)
        kinfo("%s clock time %llu", cpu_has_rdtsc() ? "TSC" : "HPET", sched_clock());
    spin_unlock(tsc_lock);
}

void arch_context_init(tcb_t thread, struct arch_context_ *context) {
    context->kernel_stack   = (uint64_t)thread + STACK_SIZE;
    context->user_stack     = context->kernel_stack;
    context->user_stack_top = context->kernel_stack;
    context->regs.rsp       = context->kernel_stack;
    context->regs.rflags    = get_rflags();
    context->regs.cs        = 0x8;
    context->regs.ss        = 0x10;
    context->regs.es        = 0x10;
    context->regs.ds        = 0x10;
    set_kernel_stack(context->kernel_stack);
    context->fs_base = read_fsbase();
    context->gs_base = read_gsbase();
    context->fs = context->gs = 0;

    context->context        = aligned_alloc(16, sizeof(struct fpu_context));
    context->context->fcw   = 0x37F;
    context->context->mxscr = 0x1F80;
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    uint64_t *stack_top              = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    new_task->context.regs.rsp       = (uint64_t)stack_top;
    new_task->context.user_stack_top = (uint64_t)stack_top;
    new_task->context.kernel_stack   = (uint64_t)stack_top;
    new_task->context.user_stack     = new_task->context.kernel_stack;

    new_task->context.regs.rip    = new_task->_start;
    new_task->context.regs.rdi    = (uint64_t)args; // first argument in rdi
    new_task->context.regs.rflags = 0x202;

    new_task->context.regs.cs = 0x8;
    new_task->context.regs.ss = 0x10;
    new_task->context.regs.es = 0x10;
    new_task->context.regs.ds = 0x10;

    new_task->context.fs_base = read_fsbase();
    new_task->context.gs_base = read_gsbase();
    new_task->context.fs = new_task->context.gs = 0;

    new_task->context.context        = aligned_alloc(16, sizeof(struct fpu_context));
    new_task->context.context->fcw   = 0x37F;
    new_task->context.context->mxscr = 0x1F80;
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    page_directory_t *dir = current->process->directory;
    if (dir != next->process->directory) {
        switch_page_directory(next->process->directory);
    }

    __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(next->context.fs));
    write_fsbase(next->context.fs_base);

    __asm__ __volatile__("movq %0, %%gs\n\t" ::"r"(next->context.gs));
    write_gsbase(next->context.gs_base);

    set_kernel_stack(next->context.kernel_stack);

    save_fpu_context(current->context.context);
    restore_fpu_context(next->context.context);

    current->context.regs.r15    = regs->r15;
    current->context.regs.r14    = regs->r14;
    current->context.regs.r13    = regs->r13;
    current->context.regs.r12    = regs->r12;
    current->context.regs.r11    = regs->r11;
    current->context.regs.r10    = regs->r10;
    current->context.regs.r9     = regs->r9;
    current->context.regs.r8     = regs->r8;
    current->context.regs.rax    = regs->rax;
    current->context.regs.rbx    = regs->rbx;
    current->context.regs.rcx    = regs->rcx;
    current->context.regs.rdx    = regs->rdx;
    current->context.regs.rdi    = regs->rdi;
    current->context.regs.rsi    = regs->rsi;
    current->context.regs.rbp    = regs->rbp;
    current->context.regs.rflags = regs->rflags;
    current->context.regs.rip    = regs->rip;
    current->context.regs.rsp    = regs->rsp;
    current->context.regs.ss     = regs->ss;
    current->context.regs.es     = regs->es;
    current->context.regs.cs     = regs->cs;
    current->context.regs.ds     = regs->ds;

    regs->r15    = next->context.regs.r15;
    regs->r14    = next->context.regs.r14;
    regs->r13    = next->context.regs.r13;
    regs->r12    = next->context.regs.r12;
    regs->r11    = next->context.regs.r11;
    regs->r10    = next->context.regs.r10;
    regs->r9     = next->context.regs.r9;
    regs->r8     = next->context.regs.r8;
    regs->rax    = next->context.regs.rax;
    regs->rbx    = next->context.regs.rbx;
    regs->rcx    = next->context.regs.rcx;
    regs->rdx    = next->context.regs.rdx;
    regs->rdi    = next->context.regs.rdi;
    regs->rsi    = next->context.regs.rsi;
    regs->rbp    = next->context.regs.rbp;
    regs->rflags = next->context.regs.rflags;
    regs->rip    = next->context.regs.rip;
    regs->rsp    = next->context.regs.rsp;
    regs->ss     = next->context.regs.ss;
    regs->es     = next->context.regs.es;
    regs->ds     = next->context.regs.ds;
    regs->cs     = next->context.regs.cs;
}

static uint64_t push_slice(uint64_t ustack, uint8_t *slice, uint64_t len) {
    uint64_t tmp_stack = ustack;
    tmp_stack -= len;
    tmp_stack -= tmp_stack % 0x08;
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
    memset(envps, 0, 1024);
    uint64_t *argvps = malloc(1024);
    memset(argvps, 0, 1024);

    if (envp != NULL) {
        for (env_i = 0; env_i < task->process->envc; env_i++) {
            tmp_stack    = push_slice(tmp_stack, (uint8_t *)envp[env_i], strlen(envp[env_i]) + 1);
            envps[env_i] = tmp_stack;
        }
    }

    for (argv_i = 0; argv_i < argc; argv_i++) {
        tmp_stack      = push_slice(tmp_stack, (uint8_t *)argv[argv_i], strlen(argv[argv_i]) + 1);
        argvps[argv_i] = tmp_stack;
    }

    uint64_t total_length = 2 * sizeof(uint64_t) + 7 * 2 * sizeof(uint64_t)
                            + (env_i + 0) * sizeof(uint64_t) + sizeof(uint64_t)
                            + (argv_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
    tmp_stack -= (tmp_stack - total_length) % 0x10;

    uint8_t random_bytes[16];
    for (int i = 0; i < 16; i++) {
        random_bytes[i] = (uint8_t)(i * 17 + 42);
    }
    tmp_stack            = push_slice(tmp_stack, random_bytes, 16);
    uint64_t random_addr = tmp_stack;

    // push auxv
    uint8_t *tmp = malloc(2 * sizeof(uint64_t));
    memset(tmp, 0, 2 * sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)src_data;
    // CP_Kernel 将用户程序本体从 0 地址加载故不加phdrs的偏移
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

#define ulog(...) logkf(__VA_ARGS__);

_Noreturn void arch_switch_to_user_mode() {
    tcb_t current                = get_current_task();
    current->context.regs.rflags = 0 << 12 | 0b10 | 1 << 9;

    pcb_t process      = current->process;
    uint64_t data_phys = 0;
    size_t data_pages  = 0;
    uint8_t *data      = NULL;
    if (process->exec == NULL) {
        ulog("process exec file handle is null.\n");
        goto err;
    }
    data_pages = (process->exec->size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (data_pages == 0) {
        data_pages = 1;
    }
    data_phys = alloc_frames(data_pages);
    if (data_phys == 0) {
        ulog(
            "cannot allocate exec buffer, size=%llu pages=%llu.\n",
            process->exec->size,
            (uint64_t)data_pages
        );
        goto err;
    }
    data = phys_to_virt(data_phys);
    memset(data, 0, data_pages * PAGE_SIZE);
    if (vfs_read(process->exec, data, 0, process->exec->size) == -1) {
        ulog("process exec read file null.\n");
        goto err_free_data;
    }

    Elf64_Ehdr *ehdr        = (Elf64_Ehdr *)data;
    uint64_t executor_start = ehdr->e_type == ET_DYN ? EXECUTOR_BASE_ADDR : 0;
    uint64_t load_start     = 0;
    void *entry = load_executor_elf(data, process->directory, executor_start, &load_start, process);
    if (entry != NULL && ehdr->e_type == ET_DYN) {
        entry = (void *)((uint64_t)entry + load_start);
    }
    if (entry == NULL) {
        ulog("cannot load process exec file.\n");
        goto err_free_data;
    }

    current->context.user_stack = page_alloc_random(
        process->directory, BIG_USER_STACK + PAGE_SIZE, PTE_PRESENT | PTE_WRITEABLE | PTE_USER
    );
    current->context.user_stack_top = current->context.user_stack + BIG_USER_STACK;
    uint64_t rsp                    = current->context.user_stack_top;

    vma_t *stack_vma = vma_alloc();

    stack_vma->vm_start = current->context.user_stack;
    stack_vma->vm_end   = current->context.user_stack_top;
    stack_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

    stack_vma->vm_type = VMA_TYPE_ANON;
    stack_vma->vm_name = strdup("[stack]");

    vma_t *region = vma_find_intersection(
        &process->vma_manager, current->context.user_stack, current->context.user_stack_top
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
            data, process->directory, &linker_start, &link_data, &link_size, &link_phys, &link_pages
        );
        if (linker_main == NULL) {
            logkf("elf_load: Cannot load libc module.\n\r");
            goto err_free_data;
        }

        uintptr_t lm_offset = (uintptr_t)linker_main + linker_start;
        linker_main         = (void *)lm_offset;

        // VMA 标记
        vma_t *ld_so_vma = vma_alloc();

        ld_so_vma->vm_start = linker_start;
        ld_so_vma->vm_end   = linker_start + link_size;
        ld_so_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

        ld_so_vma->vm_type = VMA_TYPE_ANON;
        ld_so_vma->vm_name = strdup("[libc]");

        vma_t *region =
            vma_find_intersection(&process->vma_manager, linker_start, linker_start + link_size);
        if (!region) {
            vma_insert(&process->vma_manager, ld_so_vma);
        }
        // 如未实现 VMA 可以直接去掉这段代码

        rsp = build_user_stack(
            current,
            rsp,
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
        rsp = build_user_stack(current, rsp, (uint64_t)entry, 0, NULL, 0, 0, 0, data, load_start);
    }
    free_frames(data_phys, data_pages);
    arch_close_interrupt();
    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)),
                       "r"(rsp),
                       "r"(current->context.regs.rflags),
                       "r"((uint64_t)0x23),
                       "r"(entry),
                       "r"((uint64_t)0x1b)
                     : "memory");
err_free_data:
    if (data_phys != 0) {
        free_frames(data_phys, data_pages);
    }
err:
    arch_open_interrupt();
    if (process->child_threads->size <= 1) {
        kill_proc(process, -1, true);
    } else
        kill_thread(current);
    while (true)
        arch_wait_for_interrupt();
}

void arch_context_free(tcb_t thread) {
    free(thread->context.context);
}

bool arch_check_user_mode(const struct pt_regs *regs) {
    return (regs->cs & 0x03) == 3;
}
