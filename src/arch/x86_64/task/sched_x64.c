#include "fsgsbase.h"
#include "hpet.h"
#include "io.h"
#include "lock.h"
#include "ptrace.h"
#include "task/task.h"
#include "task/smp.h"
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
    //    uint32_t low, high;
    //    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    //    return ((uint64_t)high << 32) | low;
    __asm__ volatile("");
    return __builtin_ia32_rdtsc();
}

size_t sched_clock() {
    if (!arch_current_cpu()->arch_data.support_tsc) return nano_time();
    uint64_t now   = read_tsc();
    uint64_t delta = now - arch_current_cpu()->arch_data.tsc_base_tsc;
    uint64_t ns    = ((delta * (uint64_t)arch_current_cpu()->arch_data.tsc_conv_mul) >>
                   arch_current_cpu()->arch_data.tsc_conv_shift);
    return (ns - arch_current_cpu()->arch_data.tsc_base_tsc) / 1000;
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
    context->kernel_stack  = get_rsp();
    context->user_stack    = get_rsp();
    context->regs.rflags   = get_rflags();
    context->signal_stack  = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    context->syscall_stack = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    set_kernel_stack(get_rsp());
    context->fs_base = read_fsbase();
    context->gs_base = read_gsbase();
    context->fs = context->gs = 0;
}

void arch_context_init_thread(tcb_t new_task,void *args) {
    uint64_t *stack_top              = (uint64_t *)((uint64_t)new_task + STACK_SIZE);
    new_task->context.regs.rsp       = (uint64_t)stack_top;
    new_task->context.user_stack_top = (uint64_t)stack_top;
    new_task->context.kernel_stack   = (uint64_t)stack_top;
    new_task->context.signal_stack   = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    new_task->context.syscall_stack  = (uint64_t)aligned_alloc(PAGE_SIZE, STACK_SIZE) + STACK_SIZE;
    new_task->context.user_stack     = new_task->context.kernel_stack;

    new_task->context.regs.rip    = (uint64_t)new_task->_start;
    new_task->context.regs.rdi    = (uint64_t)args; // first argument in rdi
    new_task->context.regs.rflags = 0x202;

    new_task->context.regs.cs = 0x8;
    new_task->context.regs.ss = 0x10;
    new_task->context.regs.es = 0x10;
    new_task->context.regs.ds = 0x10;
}

void arch_task_switch(tcb_t current, tcb_t next, struct pt_regs *regs) {
    page_directory_t *dir = get_current_task()->process->directory;
    if (dir != next->process->directory) { switch_page_directory(dir); }

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

_Noreturn void arch_switch_to_user_mode() {
    pcb_t process = get_current_task()->process;
    while (true) arch_wait_for_interrupt();
}
