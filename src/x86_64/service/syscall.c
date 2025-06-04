#include "syscall.h"
#include "errno.h"
#include "frame.h"
#include "fsgsbase.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "page.h"
#include "pcb.h"
#include "scheduler.h"
#include "signal.h"
#include "time.h"
#include "vfs.h"

extern lock_queue *pgb_queue;

USED uint64_t get_kernel_stack() {
    return get_current_task()->kernel_stack;
}

__attribute__((naked)) void asm_syscall_handle() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "cld\n\t"
                     "sub rsp, 0x38\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "push rbp\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push r8\n\t"
                     "push r9\n\t"
                     "push r10\n\t"
                     "push r11\n\t"
                     "push r12\n\t"
                     "push r13\n\t"
                     "push r14\n\t"
                     "push r15\n\t"
                     "mov r15, rsp\n\t"
                     "call get_kernel_stack\n\t"
                     "sub rax, 0xc0\n\t"
                     "mov rsp, rax\n\t"
                     "mov rdi, rsp\n\t"
                     "mov rsi, r15\n\t"
                     "mov rdx, 0xc0\n\t"
                     "call memcpy\n\t"
                     "mov rdi, rsp\n\t"
                     "mov rsi, r15\n\t"
                     "call syscall_handler\n\t"
                     "mov rdi, r15\n\t"
                     "mov rsi, rsp\n\t"
                     "mov rdx, 0xc0\n\t"
                     "call memcpy\n\t"
                     "mov rsp, rax\n\t"
                     "pop r15\n\t"
                     "pop r14\n\t"
                     "pop r13\n\t"
                     "pop r12\n\t"
                     "pop r11\n\t"
                     "pop r10\n\t"
                     "pop r9\n\t"
                     "pop r8\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop rbp\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "add rsp, 0x38\n\t"
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

static inline void enable_syscall() {
    uint64_t efer;
    efer           = rdmsr(MSR_EFER);
    efer          |= 1;
    uint64_t star  = ((uint64_t)((0x18 | 0x3) - 8) << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_EFER, efer);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)asm_syscall_handle);
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}

syscall_(exit) {
    int   exit_code   = arg0;
    tcb_t exit_thread = get_current_task();
    logkf("Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    kill_thread(exit_thread);
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(open) {
    char *path = (char *)arg0;
    if (path == NULL) return SYSCALL_FAULT_(EINVAL);
    logkf("syscall open: %s\n", path);

    vfs_node_t node  = vfs_open(path);
    int        index = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, node);
    spin_unlock(get_current_task()->parent_group->file_open->lock);
    if (index == -1) {
        logkf("syscall open: %s failed.\n", path);
        vfs_free(node);
        return SYSCALL_FAULT_(ENOENT);
    }
    return index;
}

syscall_(close) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = (vfs_node_t)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_free(node);
    return SYSCALL_SUCCESS;
}

syscall_(write) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    return vfs_write(node, buffer, 0, arg2);
}

syscall_(read) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t   *buffer = (uint8_t *)arg1;
    vfs_node_t node   = queue_get(get_current_task()->parent_group->file_open, fd);
    int        ret    = vfs_read(node, buffer, 0, arg2);
    return ret;
}

syscall_(waitpid) {
    int pid = arg0;
    if (pid < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    if (found_pcb(pid) == NULL) return SYSCALL_FAULT_(ESRCH);
    int *status = (int *)arg1;
    *status     = waitpid(pid);
    return SYSCALL_SUCCESS;
}

syscall_(mmap) {
    uint64_t addr   = arg0;
    size_t   length = arg1;
    uint64_t prot   = arg2;
    uint64_t flags  = arg3;
    int      fd     = (int)arg4;
    uint64_t offset = arg5;

    uint64_t aligned_len = PADDING_UP(length, PAGE_SIZE);
    if (aligned_len == 0) { return SYSCALL_FAULT_(EINVAL); }

    pcb_t process = get_current_task()->parent_group;

    uint64_t count = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (count == 0) return EOK;

    if (addr == 0) {
        addr                 = process->mmap_start;
        flags               &= (~MAP_FIXED);
        process->mmap_start += aligned_len;
        if (process->mmap_start > USER_MMAP_END) {
            process->mmap_start -= aligned_len;
            return SYSCALL_FAULT_(ENOMEM);
        }
    } else
        process->mmap_start += aligned_len;

    if (addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(EACCES); // 不允许映射到内核地址空间
    if (!(flags & MAP_ANONYMOUS)) return SYSCALL_FAULT_(EINVAL);
    uint64_t vaddr      = addr & ~(PAGE_SIZE - 1);
    uint64_t page_flags = PTE_PRESENT | PTE_USER | PTE_WRITEABLE;
    if (prot & PROT_READ) { page_flags |= PTE_PRESENT; }
    if (prot & PROT_WRITE) { page_flags |= PTE_WRITEABLE; }
    if (prot & PROT_EXEC) { page_flags |= PTE_USER; }

    // 预分配机制, 只有用户程序访问这段区域才进行实际分配(具体如何分配看page.c/page_fault_handle)
    mm_virtual_page_t *virt_page = malloc(sizeof(mm_virtual_page_t));
    not_null_assets(virt_page, "Out of memory for virtual page allocation");
    virt_page->start     = vaddr;
    virt_page->count     = count;
    virt_page->flags     = flags;
    virt_page->pte_flags = page_flags;
    virt_page->index     = queue_enqueue(process->virt_queue, virt_page);

    if (fd > 2) {
        vfs_node_t file = (vfs_node_t)queue_get(get_current_task()->parent_group->file_open, fd);
        if (!file) return SYSCALL_FAULT_(EBADF);
        vfs_read(file, (void *)addr, offset, length);
    }

    return addr;
}

syscall_(signal) {
    int sig = arg0;
    if (sig < 0 || sig >= MAX_SIGNALS) return SYSCALL_FAULT_(EINVAL);
    void *handler = (void *)arg1;
    if (handler == NULL) return SYSCALL_FAULT_(EINVAL);
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
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, fd);
    return node->size;
}

syscall_(clone) {
    close_interrupt;
    disable_scheduler();
    uint64_t flags      = arg0;
    uint64_t stack      = arg1;
    int     *parent_tid = (int *)arg2;
    int     *child_tid  = (int *)arg3;
    uint64_t tls        = arg4;
    uint64_t id         = thread_clone(regs, flags, stack, parent_tid, child_tid, tls);
    open_interrupt;
    enable_scheduler();
    return id;
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
    scheduler_yield();
    return SYSCALL_SUCCESS;
}

syscall_(uname) {
    if (arg0 == 0) return SYSCALL_FAULT;
    struct utsname *utsname   = (struct utsname *)arg0;
    char            sysname[] = "CoolPotOS";
    char            machine[] = "x86_64";
    char            version[] = "0.0.1";
    memcpy(utsname->sysname, sysname, sizeof(sysname));
    memcpy(utsname->nodename, get_current_task()->parent_group->user->name, 50);
    memcpy(utsname->release, KERNEL_NAME, sizeof(KERNEL_NAME));
    memcpy(utsname->version, version, sizeof(version));
    memcpy(utsname->machine, machine, sizeof(machine));
    return SYSCALL_SUCCESS;
}

syscall_(nano_sleep) {
    struct timespec k_req;
    if (arg0 == 0) return SYSCALL_FAULT;
    memcpy(&k_req, (void *)arg0, sizeof(k_req));
    if (k_req.tv_nsec >= 1000000000L) return SYSCALL_FAULT;
    uint64_t nsec = (uint64_t)k_req.tv_sec * 1000000000ULL + k_req.tv_nsec;
    scheduler_nano_sleep(nsec);
    return SYSCALL_SUCCESS;
}

syscall_(ioctl) {
    int fd      = (int)arg0;
    int options = (size_t)arg1;
    if (fd < 0 || arg2 == 0) return SYSCALL_FAULT;
    vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, fd);
    int        ret  = vfs_ioctl(node, options, (void *)arg2);
    return ret;
}

syscall_(writev) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return -(uint64_t)ENOSTR;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    int           iovcnt = arg2;
    struct iovec *iov    = (struct iovec *)arg1;
    vfs_node_t    node   = queue_get(get_current_task()->parent_group->file_open, fd);
    ssize_t       total  = 0;
    for (int i = 0; i < iovcnt; i++) {
        int status = vfs_write(node, iov[i].iov_base, 0, iov[i].iov_len);
        if (status == VFS_STATUS_FAILED) return total;
        total += iov[i].iov_len;
    }
    return total;
}

syscall_(readv) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return ENODEV;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    size_t        iovcnt  = arg2;
    struct iovec *iov     = (struct iovec *)arg1;
    vfs_node_t    node    = queue_get(get_current_task()->parent_group->file_open, fd);
    size_t        buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf    = (uint8_t *)malloc(buf_len);
    size_t   status = vfs_read(node, buf, 0, buf_len);
    if (status == (size_t)VFS_STATUS_FAILED) {
        free(buf);
        return SYSCALL_FAULT_(EIO);
    }
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        size_t len = iov[i].iov_len;
        if (len == 0) continue;

        size_t to_copy = len;
        if (copied + to_copy > status) { to_copy = status - copied; }

        memcpy(iov[i].iov_base, buf + copied, to_copy);
        copied += to_copy;
    }
    free(buf);
    return status;
}

syscall_(munmap) {

    return SYSCALL_FAULT_(ENOSYS);
}

syscall_(mremap) {
    return SYSCALL_FAULT_(ENOSYS);
}

syscall_(getcwd) {
    char  *buffer = (char *)arg0;
    size_t length = arg1;
    if (buffer == NULL) return SYSCALL_FAULT_(EINVAL);
    if (length == 0) return SYSCALL_SUCCESS;
    pcb_t  process  = get_current_task()->parent_group;
    char  *cwd      = process->cwd;
    size_t cwd_leng = strlen(cwd);
    if (length > cwd_leng) length = cwd_leng;
    memcpy(buffer, cwd, length);
    return length;
}

syscall_(chdir) {
    char *s = (char *)arg0;
    if (s == NULL) return SYSCALL_FAULT_(EINVAL);
    pcb_t process = get_current_task()->parent_group;

    char *path;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        path = pathacat(process->cwd, s);
    }

    char *normalized_path = normalize_path(path);
    free(path);

    if (normalized_path == NULL) { return SYSCALL_FAULT_(ENOMEM); }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL) {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (node->type == file_dir) {
        strcpy(process->cwd, normalized_path);
    } else {
        return SYSCALL_FAULT_(ENOTDIR);
    }

    free(normalized_path);
    return SYSCALL_SUCCESS;
}

syscall_(exit_group) {
    int   exit_code    = arg0;
    pcb_t exit_process = get_current_task()->parent_group;
    logkf("Process %s exit with code %d.\n", exit_process->name, exit_code);
    kill_proc(exit_process, exit_code);
    cpu_hlt;
}

syscall_(poll) {
    struct pollfd *fds_user = (struct pollfd *)arg0;
    size_t         nfds     = arg1;
    int            timeout  = arg2;
    UNUSED(timeout); //TODO 设备状态超时等待

    struct pollfd *local_fds = (struct pollfd *)malloc(sizeof(struct pollfd) * nfds);
    memcpy(fds_user, local_fds, sizeof(struct pollfd) * nfds);

    int num_ready = 0;
    for (size_t i = 0; i < nfds; ++i) {
        vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, local_fds[i].fd);
        if (node == NULL) {
            local_fds[i].revents = POLLNVAL;
            num_ready++;
            continue;
        }
        short revents = 0;

        if ((local_fds[i].events & POLLIN)) revents |= POLLIN;

        if ((local_fds[i].events & POLLOUT)) revents |= POLLOUT;

        local_fds[i].revents = revents;

        if (revents) num_ready++;
    }

    return num_ready;
}

syscall_(set_tid_address) {
    int *tidptr = (int *)arg0;
    if (tidptr == NULL) return SYSCALL_FAULT_(EINVAL);
    tcb_t thread          = get_current_task();
    thread->tid_address   = (uint64_t)tidptr;
    thread->tid_directory = get_current_directory();
    return SYSCALL_SUCCESS;
}

syscall_(sched_getaffinity) {
    int    pid      = (int)arg0;
    size_t set_size = (size_t)arg1;
    return SYSCALL_FAULT_(ENOSYS);
}

syscall_(rt_sigprocmask) {
    int       how        = (int)arg0;
    sigset_t *set        = (sigset_t *)arg1;
    sigset_t *oldset     = (sigset_t *)arg2;
    size_t    sigsetsize = (size_t)arg3;
    pcb_t     process    = get_current_task()->parent_group;

    if (sigsetsize != sizeof(sigset_t)) return SYSCALL_FAULT_(EINVAL);

    if (oldset) {
        sigset_t old_mask = 0;
        for (int i = 0; i < MAX_SIGNALS; ++i) {
            if (process->task_signal->signal_mask[i]) { old_mask |= (1ULL << i); }
        }
        *oldset = old_mask;
    }

    if (set == NULL) { return SYSCALL_SUCCESS; }
    sigset_t new_set = *set;

    switch (how) {
    case SIG_BLOCK:
        for (int i = 0; i < MAX_SIGNALS; ++i) {
            if (new_set & (1ULL << i)) { process->task_signal->signal_mask[i] = true; }
        }
        break;

    case SIG_UNBLOCK:
        for (int i = 0; i < MAX_SIGNALS; ++i) {
            if (new_set & (1ULL << i)) { process->task_signal->signal_mask[i] = false; }
        }
        break;

    case SIG_SETMASK:
        for (int i = 0; i < MAX_SIGNALS; ++i) {
            process->task_signal->signal_mask[i] = (new_set & (1ULL << i)) != 0;
        }
        break;

    default: return SYSCALL_FAULT_(EINVAL);
    }

    return SYSCALL_SUCCESS;
}

syscall_(dup2) {
    int        fd    = (int)arg0;
    int        newfd = (int)arg1;
    vfs_node_t node  = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!node) return (uint64_t)-EBADF;

    vfs_node_t new = vfs_dup(node);
    if (!new) return (uint64_t)-ENOSPC;
    vfs_node_t new_node = queue_get(get_current_task()->parent_group->file_open, newfd);
    if (new_node != NULL) { vfs_close(new_node); }
    queue_enqueue_id(get_current_task()->parent_group->file_open, new, newfd);
    return newfd;
}

syscall_(dup) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, fd);
    if (node == NULL) return SYSCALL_FAULT_(EBADF);
    uint64_t i;
    for (i = 3; i < INT32_MAX; i++) {
        vfs_node_t node0 = queue_get(get_current_task()->parent_group->file_open, fd);
        if (node0 == NULL) { break; }
    }
    return syscall_dup2(fd, i, 0, 0, 0, 0, regs);
}

syscall_(fcntl) {
    int      fd  = (int)arg0;
    int      cmd = (int)arg1;
    uint64_t arg = arg2;
    if (fd < 0 || cmd < 0) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = queue_get(get_current_task()->parent_group->file_open, fd);
    if (node == NULL) return SYSCALL_FAULT_(EBADF);

    switch (cmd) {
    case F_GETFD: return !!(node->flags & O_CLOEXEC);
    case F_SETFD: return node->flags |= O_CLOEXEC;
    case F_DUPFD_CLOEXEC:
        uint64_t newfd  = syscall_dup(fd, 0, 0, 0, 0, 0, regs);
        node->flags    |= O_CLOEXEC;
        return newfd;
    case F_DUPFD: return syscall_dup(fd, 0, 0, 0, 0, 0, regs);
    case F_GETFL: return node->flags;
    case F_SETFL:
        uint32_t valid_flags  = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        node->flags          &= ~valid_flags;
        node->flags          |= arg & valid_flags;
    }
    return SYSCALL_SUCCESS;
}

syscall_(sigaltstack) {
    altstack_t *old_stack = (altstack_t *)arg0;
    altstack_t *new_stack = (altstack_t *)arg1;

    tcb_t thread = get_current_task();
    if (old_stack) { *old_stack = thread->alt_stack; }
    if (new_stack) { thread->alt_stack = *new_stack; }
    return SYSCALL_SUCCESS;
}

syscall_(sigaction) {
    int               sig    = (int)arg0;
    struct sigaction *act    = (struct sigaction *)arg1;
    struct sigaction *oldact = (struct sigaction *)arg2;
    return signal_action(sig, act, oldact);
}

syscall_(fork) {
    tcb_t parent = get_current_task();
    tcb_t child  = (tcb_t)malloc(STACK_SIZE);
    return SYSCALL_FAULT_(ENOSYS);
}

syscall_(futex) {
    int             *uaddr   = (int *)arg0;
    int              op      = (int)arg1;
    int              val     = (int)arg2;
    struct timespec *time    = (struct timespec *)arg3;
    int              timeout = (int)arg4;
    tcb_t            thread  = get_current_task();

    bool is_pre_sub = false;
re_futex: //TODO PRIVATE 标志暂时不支持
    switch (op) {
    case FUTEX_WAIT:
        if (uaddr == NULL) return SYSCALL_FAULT_(EINVAL);
        int observed = *uaddr;
        if (observed != val) { return SYSCALL_FAULT_(EAGAIN); }
        thread->status = FUTEX; // 挂起当前线程
        futex_add((void *)page_virt_to_phys((uint64_t)uaddr), thread);
        scheduler_yield();
        return SYSCALL_SUCCESS;
    case FUTEX_WAKE:
        futex_wake((void *)page_virt_to_phys((uint64_t)uaddr), val);
        return SYSCALL_SUCCESS;
    default: {
        if (!is_pre_sub) {
            op         -= 1;
            is_pre_sub  = true;
            goto re_futex;
        }
        return SYSCALL_FAULT_(EINVAL);
    }
    }
}

syscall_(get_tid) {
    return get_current_task()->pid;
}

syscall_(mprotect) {
    uint64_t addr   = arg0;
    size_t   length = arg1;
    uint64_t prot   = arg2;

    if (addr == 0 || length == 0) return SYSCALL_FAULT_(EINVAL);
    if (addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(EACCES);

    addr              = PADDING_DOWN(addr, PAGE_SIZE);
    size_t page_count = PADDING_UP(length, PAGE_SIZE) / PAGE_SIZE;
    pcb_t  process    = get_current_task()->parent_group;

    for (size_t i = 0; i < page_count; i++) {
        uint64_t page_addr = addr + i * PAGE_SIZE;

        bool updated = false;

        spin_lock(process->virt_queue->lock);
        queue_foreach(process->virt_queue, node) {
            mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
            if (page_addr >= vpage->start && page_addr < vpage->start + vpage->count * PAGE_SIZE) {
                uint64_t new_flags = 0;
                if (prot & PROT_READ) new_flags |= PTE_PRESENT;
                if (prot & PROT_WRITE) new_flags |= PTE_WRITEABLE;
                if (prot & PROT_EXEC) new_flags |= PTE_USER;

                vpage->pte_flags = new_flags;
                updated          = true;
                break;
            }
        }
        spin_unlock(process->virt_queue->lock);

        if (!updated) {
            uint64_t old_flags;
            if (!page_table_get_flags(get_current_directory(), page_addr, &old_flags)) {
                return SYSCALL_FAULT_(ENOMEM);
            }

            uint64_t new_flags = old_flags & ~(PTE_WRITEABLE | PTE_USER);
            if (prot & PROT_READ) new_flags |= PTE_PRESENT;
            if (prot & PROT_WRITE) new_flags |= PTE_WRITEABLE;
            if (prot & PROT_EXEC) new_flags |= PTE_USER;

            page_table_update_flags(get_current_directory(), page_addr, new_flags);
        }
    }

    return SYSCALL_SUCCESS;
}

// clang-format off
syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT]        = syscall_exit,
    [SYSCALL_EXIT_GROUP]  = syscall_exit_group,
    [SYSCALL_OPEN]        = syscall_open,
    [SYSCALL_CLOSE]       = syscall_close,
    [SYSCALL_WRITE]       = syscall_write,
    [SYSCALL_READ]        = syscall_read,
    [SYSCALL_WAITPID]     = syscall_waitpid,
    [SYSCALL_MMAP]        = syscall_mmap,
//TODO [SYSCALL_SIGNAL]      = syscall_signal,
    [SYSCALL_SIGRET]      = syscall_sigret,
    [SYSCALL_GETPID]      = syscall_getpid,
    [SYSCALL_PRCTL]       = syscall_prctl,
    [SYSCALL_STAT]        = syscall_size,
    [SYSCALL_CLONE]       = syscall_clone,
    [SYSCALL_ARCH_PRCTL]  = syscall_arch_prctl,
    [SYSCALL_YIELD]       = syscall_yield,
    [SYSCALL_UNAME]       = syscall_uname,
    [SYSCALL_NANO_SLEEP]  = syscall_nano_sleep,
    [SYSCALL_IOCTL]       = syscall_ioctl,
    [SYSCALL_WRITEV]      = syscall_writev,
    [SYSCALL_READV]       = syscall_readv,
    [SYSCALL_MUNMAP]      = syscall_munmap,
    [SYSCALL_MREMAP]      = syscall_mremap,
    [SYSCALL_GETCWD]      = syscall_getcwd,
    [SYSCALL_CHDIR]       = syscall_chdir,
    [SYSCALL_POLL]        = syscall_poll,
    [SYSCALL_SETID_ADDR]  = syscall_set_tid_address,
    [SYSCALL_G_AFFINITY]  = syscall_sched_getaffinity,
    [SYSCALL_RT_SIGMASK]  = syscall_rt_sigprocmask,
    [SYSCALL_FCNTL]       = syscall_fcntl,
    [SYSCALL_DUP]         = syscall_dup,
    [SYSCALL_DUP2]        = syscall_dup2,
    [SYSCALL_SIGALTSTACK] = syscall_sigaltstack,
    [SYSCALL_SIGACTION]   = syscall_sigaction,
    [SYSCALL_FORK]        = syscall_fork,
    [SYSCALL_FUTEX]       = syscall_futex,
    [SYSCALL_GET_TID]     = syscall_get_tid,
    [SYSCALL_MPROTECT]    = syscall_mprotect,
};
// clang-format on

USED void syscall_handler(struct syscall_regs *regs,
                          struct syscall_regs *user_regs) { // syscall 指令处理
    regs->rip    = regs->rcx;
    regs->rflags = regs->r11;
    regs->cs     = (0x20 | 0x3);
    regs->ss     = (0x18 | 0x3);
    regs->ds     = (0x18 | 0x3);
    regs->es     = (0x18 | 0x3);
    regs->rsp    = (uint64_t)(user_regs + 1);
    write_fsbase((uint64_t)get_current_task());
    uint64_t syscall_id = regs->rax & 0xFFFFFFFF;

    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        regs->rax = ((syscall_t)syscall_handlers[syscall_id])(regs->rdi, regs->rsi, regs->rdx,
                                                              regs->r10, regs->r8, regs->r9, regs);
    } else
        regs->rax = SYSCALL_FAULT;
    logkf("SYScall: %d RET:%d\n", syscall_id, regs->rax);
    write_fsbase(get_current_task()->fs_base);
}

USED registers_t *syscall_handle(registers_t *reg) { // int 0x80 软中断处理
    open_interrupt;
    write_fsbase((uint64_t)get_current_task());
    size_t              syscall_id = reg->rax;
    struct syscall_regs regs;
    regs.rax    = reg->rax;
    regs.rdi    = reg->rdi;
    regs.rsi    = reg->rsi;
    regs.rdx    = reg->rdx;
    regs.r10    = reg->r10;
    regs.r8     = reg->r8;
    regs.r9     = reg->r9;
    regs.r15    = reg->r15;
    regs.r14    = reg->r14;
    regs.r13    = reg->r13;
    regs.r12    = reg->r12;
    regs.r11    = reg->r11;
    regs.rbx    = reg->rbx;
    regs.rcx    = reg->rcx;
    regs.rbp    = reg->rbp;
    regs.ds     = reg->ds;
    regs.es     = reg->es;
    regs.rip    = reg->rip;
    regs.cs     = reg->cs;
    regs.rflags = reg->rflags;
    regs.rsp    = reg->rsp;
    regs.ss     = reg->ss;
    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        reg->rax = ((syscall_t)syscall_handlers[syscall_id])(reg->rdi, reg->rsi, reg->rdx, reg->r10,
                                                             reg->r8, reg->r9, &regs);
        reg->rdi = regs.rdi;
        reg->rsi = regs.rsi;
        reg->rdx = regs.rdx;
        reg->r10 = regs.r10;
        reg->r8  = regs.r8;
        reg->r9  = regs.r9;
        reg->r15 = regs.r15;
        reg->r14 = regs.r14;
        reg->r13 = regs.r13;
        reg->r12 = regs.r12;
        reg->r11 = regs.r11;
        reg->rbx = regs.rbx;
        reg->rcx = regs.rcx;
        reg->rbp = regs.rbp;
        reg->ds  = regs.ds;
        reg->es  = regs.es;
        reg->rip = regs.rip;
    } else
        reg->rax = SYSCALL_FAULT;

    write_fsbase(get_current_task()->fs_base);
    return reg;
}

void setup_syscall(bool is_print) {
    enable_syscall();
    register_interrupt_handler(0x80, asm_syscall_entry, 0, 0x8E | 0x60);
    if (is_print) kinfo("Setup CP_Kernel syscall table.");
}
