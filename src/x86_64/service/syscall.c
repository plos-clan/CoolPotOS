#include "syscall.h"
#include "errno.h"
#include "frame.h"
#include "fsgsbase.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "page.h"
#include "pcb.h"
#include "scheduler.h"
#include "signal.h"
#include "vfs.h"

extern void asm_syscall_entry();

extern lock_queue *pgb_queue;

static inline void enable_syscall() {
    uint64_t efer;
    efer  = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
}

__attribute__((naked)) void asm_syscall_handle() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "swapgs\n\t"
                     "call syscall_handle\n\t"
                     "swapgs\n\t"
                     "sysretq\n\t");
}

__attribute__((naked)) void asm_syscall_entry() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "push 0\n\t" // 对齐
                     "push 0\n\t" // 对齐
                     "swapgs\n\t"
                     "push r15\n\t"
                     "push r14\n\t"
                     "push r13\n\t"
                     "push r12\n\t"
                     "push r11\n\t"
                     "push r10\n\t"
                     "push r9\n\t"
                     "push r8\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rbp\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "mov rdi, rsp\n\t"
                     "call syscall_handle\n\t"
                     "mov rsp, rax\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rbp\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop r8\n\t"
                     "pop r9\n\t"
                     "pop r10\n\t"
                     "pop r11\n\t"
                     "pop r12\n\t"
                     "pop r13\n\t"
                     "pop r14\n\t"
                     "pop r15\n\t"
                     "add rsp, 16\n\t" // 越过对齐
                     "swapgs\n\t"
                     "sti\n\t"
                     "iretq\n\t");
}

syscall_(exit) {
    int exit_code = arg0;
    logkf("Process %s exit with code %d.\n", get_current_task()->parent_group->name, exit_code);
    kill_proc(get_current_task()->parent_group, exit_code);
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(abort) {
    logkf("Process %s abort.\n", get_current_task()->parent_group->name);
    kill_proc(get_current_task()->parent_group, -1);
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(open) {
    char *path = (char *)arg0;
    if (path == NULL) return -1;

    logkf("syscall open: %s\n", path);

    vfs_node_t node  = vfs_open(path);
    int        index = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, node);
    spin_unlock(get_current_task()->parent_group->file_open->lock);
    if (index == -1) {
        logkf("syscall open: %s failed.\n", path);
        vfs_free(node);
        return SYSCALL_FAULT;
    }
    return index;
}

syscall_(close) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT;
    vfs_node_t node = (vfs_node_t)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_free(node);
    return SYSCALL_SUCCESS;
}

syscall_(write) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    return vfs_write(node, buffer, 0, arg2);
}

syscall_(read) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    int        ret    = vfs_read(node, buffer, 0, arg2);
    return ret;
}

syscall_(waitpid) {
    int pid = arg0;
    if (pid < 0 || arg1 == 0) return SYSCALL_FAULT;
    if (found_pcb(pid) == NULL) return SYSCALL_FAULT;
    int *status = (int *)arg1;
    *status     = waitpid(pid);
    return SYSCALL_SUCCESS;
}

syscall_(mmap) {
    uint64_t addr   = arg0;
    size_t   length = arg1;
    uint64_t prot   = arg2;
    uint64_t flags  = arg3;
    if (addr == 0) return SYSCALL_FAULT;
    uint64_t count = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (count == 0) return SYSCALL_SUCCESS;

    get_current_task()->parent_group->mmap_start += (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    if (addr > KERNEL_AREA_MEM) return SYSCALL_FAULT; // 不允许映射到内核地址空间
    if (!(flags & MAP_ANONYMOUS)) return SYSCALL_FAULT;
    uint64_t vaddr = addr & ~(PAGE_SIZE - 1);

    for (size_t i = 0; i < count; i++) {
        uint64_t page_flags = PTE_PRESENT | PTE_USER | PTE_WRITEABLE;
        uint64_t page_addr  = vaddr + i * PAGE_SIZE;
        if (prot & PROT_READ) { page_flags |= PTE_PRESENT; }
        if (prot & PROT_WRITE) { page_flags |= PTE_WRITEABLE; }
        if (prot & PROT_EXEC) { page_flags |= PTE_USER; }

        if (flags & MAP_FIXED) {
            page_map_to(get_current_directory(), page_addr, page_addr, page_flags);
        } else {
            uint64_t phys = alloc_frames(1);
            page_map_to(get_current_directory(), page_addr, phys, page_flags);
        }
    }

    return SYSCALL_SUCCESS;
}

syscall_(signal) {
    int sig = arg0;
    if (sig < 0 || sig >= MAX_SIGNALS) return SYSCALL_FAULT;
    void *handler = (void *)arg1;
    if (handler == NULL) return SYSCALL_FAULT;
    logkf("Signal syscall: %p\n", handler);
    register_signal(get_current_task()->parent_group, sig, handler);
    return SYSCALL_SUCCESS;
}

syscall_(sigret) {}

syscall_(getpid) {
    if (arg0 == UINT64_MAX || arg1 == UINT64_MAX || arg2 == UINT64_MAX || arg3 == UINT64_MAX ||
        arg4 == UINT64_MAX)
        return 1;
    return get_current_task()->parent_group->pgb_id;
}

syscall_(prctl) {
    return process_control(arg0, arg1, arg2, arg3, arg4);
}

syscall_(size) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT;
    vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, fd);
    return node->size;
}

syscall_(clone) {
    uint64_t flags = arg0;
    uint64_t stack = arg1;
    uint64_t pt    = arg2;
    //TODO 待实现
    return SYSCALL_FAULT;
}

syscall_(arch_prctl) {
    uint64_t code   = arg0;
    uint64_t addr   = arg1;
    tcb_t    thread = get_current_task();
    switch (code) {
    case ARCH_SET_FS:
        thread->fs_base = addr;
        write_fsbase(thread->fs_base);
        break;
    case ARCH_GET_FS: return thread->fs_base;
    case ARCH_SET_GS:
        thread->gs_base = addr;
        write_gsbase(thread->gs_base);
        break;
    case ARCH_GET_GS: return thread->gs_base;
    default: return SYSCALL_FAULT;
    }
    return SYSCALL_SUCCESS;
}

syscall_(yield) {
    __asm__ volatile("int %0" ::"i"(timer));
    return SYSCALL_SUCCESS;
}

syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT]       = syscall_exit,
    [SYSCALL_ABORT]      = syscall_abort,
    [SYSCALL_OPEN]       = syscall_open,
    [SYSCALL_CLOSE]      = syscall_close,
    [SYSCALL_WRITE]      = syscall_write,
    [SYSCALL_READ]       = syscall_read,
    [SYSCALL_WAITPID]    = syscall_waitpid,
    [SYSCALL_MMAP]       = syscall_mmap,
    [SYSCALL_SIGNAL]     = syscall_signal,
    [SYSCALL_SIGRET]     = syscall_sigret,
    [SYSCALL_GETPID]     = syscall_getpid,
    [SYSCALL_PRCTL]      = syscall_prctl,
    [SYSCALL_SIZE]       = syscall_size,
    [SYSCALL_CLONE]      = syscall_clone,
    [SYSCALL_ARCH_PRCTL] = syscall_arch_prctl,
    [SYSCALL_YIELD]      = syscall_yield,
};

USED registers_t *syscall_handle(registers_t *reg) {
    write_fsbase((uint64_t)get_current_task());
    open_interrupt;
    size_t syscall_id = reg->rax;
    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        reg->rax = ((syscall_t)syscall_handlers[syscall_id])(reg->rbx, reg->rcx, reg->rdx, reg->rsi,
                                                             reg->rdi);
    } else
        reg->rax = SYSCALL_FAULT;
    write_fsbase(get_current_task()->fs_base);
    return reg;
}

void setup_syscall() {
    UNUSED(enable_syscall);
    register_interrupt_handler(0x80, asm_syscall_entry, 0, 0x8E | 0x60);
    kinfo("Setup CP_Kernel syscall table.");
}
