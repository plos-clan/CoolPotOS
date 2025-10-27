#include "exec/elf_load.h"
#include "fs/vfs.h"
#include "fsgsbase.h"
#include "hpet.h"
#include "intctl.h"
#include "io.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/page.h"
#include "ptrace.h"
#include "task/smp.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

spin_t tsc_lock = SPIN_INIT;

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
    __asm__ volatile("");
    return __builtin_ia32_rdtsc();
}

size_t sched_clock() {
    return nano_time(); //TODO 优先 nano_time 提供, tsc计算有误暂时废弃
    if (!arch_current_cpu()->arch_data.support_tsc) return nano_time();
    uint64_t now   = read_tsc();
    uint64_t delta = now - arch_current_cpu()->arch_data.tsc_base_tsc;
    uint64_t ns    = ((delta * (uint64_t)arch_current_cpu()->arch_data.tsc_conv_mul) >>
                   arch_current_cpu()->arch_data.tsc_conv_shift);
    return (ns - arch_current_cpu()->arch_data.tsc_base_tsc) / 1000;
}

void arch_send_scheduler(){
    __asm__ volatile("int %0\n\r" :: "i"(timer));
}

void calibrate_tsc_with_hpet() {
    spin_lock(tsc_lock);
    bool is_bsp                               = arch_current_cpu()->id == get_bsp_cpu_id();
    arch_current_cpu()->arch_data.support_tsc = cpu_has_rdtsc();
    if (!arch_current_cpu()->arch_data.support_tsc) goto end;
    const uint64_t target_ns = 10 * 1000 * 1000;
    uint64_t       tsc_start = read_tsc();
    uint64_t       ns_start  = nano_time();
    nsleep(target_ns);
    uint64_t tsc_end   = read_tsc();
    uint64_t ns_end    = nano_time();
    uint64_t delta_tsc = tsc_end - tsc_start;
    uint64_t delta_ns  = ns_end - ns_start;
    if (delta_tsc == 0 || delta_ns == 0) return;
    arch_current_cpu()->arch_data.tsc_conv_shift = 22;
    arch_current_cpu()->arch_data.tsc_conv_mul =
        (uint32_t)((delta_ns << arch_current_cpu()->arch_data.tsc_conv_shift) / delta_tsc);
    arch_current_cpu()->arch_data.tsc_base_tsc = read_tsc();
    arch_current_cpu()->arch_data.tsc_base_ns  = nano_time();
    uint64_t freq_hz                           = (delta_tsc * 1000000000ull) / delta_ns;
    if (is_bsp) kinfo("Estimated TSC frequency: %llu MHz", freq_hz / 1000 / 1000);
end:
    if (is_bsp) kinfo("%s clock time %llu", cpu_has_rdtsc() ? "TSC" : "HPET", sched_clock());
    spin_unlock(tsc_lock);
}

void arch_context_init(struct arch_context_ *context) {
    context->kernel_stack = get_rsp();
    context->user_stack   = get_rsp();
    context->regs.rflags  = get_rflags();
    set_kernel_stack(get_rsp());
    context->fs_base = read_fsbase();
    context->gs_base = read_gsbase();
    context->fs = context->gs = 0;
}

void arch_context_init_thread(tcb_t new_task, void *args) {
    uint64_t *stack_top              = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    new_task->context.regs.rsp       = (uint64_t)stack_top;
    new_task->context.user_stack_top = (uint64_t)stack_top;
    new_task->context.kernel_stack   = (uint64_t)stack_top;
    new_task->context.user_stack     = new_task->context.kernel_stack;

    new_task->context.regs.rip    = (uint64_t)new_task->_start;
    new_task->context.regs.rdi    = (uint64_t)args; // first argument in rdi
    new_task->context.regs.rflags = 0x202;

    new_task->context.regs.cs = 0x8;
    new_task->context.regs.ss = 0x10;
    new_task->context.regs.es = 0x10;
    new_task->context.regs.ds = 0x10;

    new_task->context.fs_base = read_fsbase();
    new_task->context.gs_base = read_gsbase();
    new_task->context.fs = new_task->context.gs = 0;
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    page_directory_t *dir = current->process->directory;
    if (dir != next->process->directory) { switch_page_directory(next->process->directory); }

    __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(next->context.fs));
    write_fsbase(next->context.fs_base);

    __asm__ __volatile__("movq %0, %%gs\n\t" ::"r"(next->context.gs));
    write_gsbase(next->context.gs_base);

    set_kernel_stack(next->context.kernel_stack);

    save_fpu_context(&current->context.context);
    restore_fpu_context(&current->context.context);

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
    uint64_t tmp_stack  = ustack;
    tmp_stack          -= len;
    tmp_stack          -= (tmp_stack % 0x08);
    memcpy((void *)tmp_stack, slice, len);
    return tmp_stack;
}

static uint64_t build_user_stack(tcb_t task, uint64_t sp, uint64_t entry_point, uint64_t link_start,
                                 uint8_t *link_data, size_t link_size, uint8_t *src_data,
                                 uint64_t load_start) {
    uint64_t env_i  = 0;
    int      argv_i = 0;

    char     *argv[50];
    char     *build_cmdline = strdup(task->process->cmdline);
    const int argc          = cmd_parse(build_cmdline, argv, ' ');

    char **envp = task->process->envp;

    uint64_t tmp_stack = sp;
    tmp_stack          = push_slice(tmp_stack, (uint8_t *)task->name, strlen(task->name) + 1);

    uint64_t execfn_ptr = tmp_stack;

    uint64_t *envps = (uint64_t *)malloc(1024);
    memset(envps, 0, 1024);
    uint64_t *argvps = (uint64_t *)malloc(1024);
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

    uint64_t total_length = 2 * sizeof(uint64_t) + 7 * 2 * sizeof(uint64_t) +
                            (env_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) +
                            (argv_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
    tmp_stack -= (tmp_stack - total_length) % 0x10;

    // push auxv
    uint8_t *tmp = (uint8_t *)malloc(2 * sizeof(uint64_t));
    memset(tmp, 0, 2 * sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    page_map_range_to_random(task->process->directory, EHDR_START_ADDR, task->process->exec->size,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    memcpy((void *)EHDR_START_ADDR, src_data, task->process->exec->size);

    if (link_data != NULL) {
        page_map_range_to_random(task->process->directory, INTERPRETER_EHDR_ADDR, link_size,
                                 PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
        memcpy((void *)INTERPRETER_EHDR_ADDR, link_data, link_size);
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)EHDR_START_ADDR;
    // CP_Kernel 将用户程序本体从 0 地址加载故不加phdrs的偏移
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(ehdr->e_phoff + load_start);

    ((uint64_t *)tmp)[0] = AT_NULL;
    ((uint64_t *)tmp)[1] = 0;

    if (link_data != NULL) {
        ((uint64_t *)tmp)[0] = AT_PHDR;
        ((uint64_t *)tmp)[1] = (uint64_t)phdrs;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

        ((uint64_t *)tmp)[0] = AT_PHENT;
        ((uint64_t *)tmp)[1] = sizeof(Elf64_Phdr);
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

        ((uint64_t *)tmp)[0] = AT_PHNUM;
        ((uint64_t *)tmp)[1] = ehdr->e_phnum;
        tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));
    }
    ((uint64_t *)tmp)[0] = AT_ENTRY;
    ((uint64_t *)tmp)[1] = entry_point;
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
    free(link_data);
    free(build_cmdline);
    cmd_free(argv, argc);

    return tmp_stack;
}

#define ulog(...) kerror(__VA_ARGS__);

_Noreturn void arch_switch_to_user_mode() {
    get_current_task()->context.regs.rflags = 0 << 12 | 0b10 | 1 << 9;

    pcb_t process = get_current_task()->process;
    if (process->exec == NULL) {
        ulog("process exec file handle is null.");
        goto err;
    }
    uint8_t *data = malloc(process->exec->size);
    if (vfs_read(process->exec, data, 0, process->exec->size) == -1) {
        ulog("process exec read file null.");
        goto err;
    }
    uint64_t load_start = 0;
    void    *entry      = load_executor_elf(data, process->directory, 0, &load_start, process);
    if (entry == NULL) {
        ulog("cannot load process exec file.");
        goto err;
    }

    get_current_task()->context.user_stack = page_alloc_random(
        process->directory, BIG_USER_STACK + PAGE_SIZE, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    get_current_task()->context.user_stack_top =
        get_current_task()->context.user_stack + BIG_USER_STACK;
    uint64_t rsp = get_current_task()->context.user_stack_top;

    if (is_dynamic((Elf64_Ehdr *)data)) {
    } else
        rsp = build_user_stack(get_current_task(), rsp, (uint64_t)entry, 0, NULL, 0, data,
                               load_start);

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
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp),
                       "r"(get_current_task()->context.regs.rflags), "r"((uint64_t)0x23),
                       "r"(entry), "r"((uint64_t)0x1b)
                     : "memory");
err:;
    while (true)
        arch_wait_for_interrupt();
}
