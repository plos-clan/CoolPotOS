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

size_t list_length(list_t list) {
    size_t count   = 0;
    list_t current = list;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

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
    char    *path0 = (char *)arg0;
    uint64_t flags = arg1;
    if (path0 == NULL) return SYSCALL_FAULT_(EINVAL);

    char *s = path0;
    char *path;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        path = pathacat(get_current_task()->parent_group->cwd, s);
    }
    char *normalized_path = normalize_path(path);
    free(path);

    logkf("syscall open: %s\n", normalized_path);

    vfs_node_t node = vfs_open(normalized_path);
    if (node == NULL) {
        if (flags & O_CREAT) {
            vfs_mkfile(path);
            node = vfs_open(path);
            if (node == NULL) goto err;
        } else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
    fd_file_handle *fd_handle = malloc(sizeof(fd_handle));
    fd_handle->offset         = 0;
    fd_handle->node           = node;
    int index     = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);
    fd_handle->fd = index;
    spin_unlock(get_current_task()->parent_group->file_open->lock);
    if (index == -1) {
        logkf("syscall open: %s failed.\n", path);
        vfs_close(node);
        free(fd_handle);
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(normalized_path);
    return index;
}

syscall_(close) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =
        (fd_file_handle *)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_close(handle->node);
    free(handle);
    return SYSCALL_SUCCESS;
}

syscall_(write) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    if (arg2 == 0) return SYSCALL_SUCCESS;
    uint8_t        *buffer = (uint8_t *)arg1;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    return vfs_write(handle->node, buffer, handle->offset, arg2);
}

syscall_(read) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return SYSCALL_FAULT_(EINVAL);
    if (arg2 == 0) return SYSCALL_SUCCESS;
    logkf("fd = %d, arg1 = %p, arg2 = %d\n", fd, arg1, arg2);
    uint8_t        *buffer = (uint8_t *)arg1;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    int ret = vfs_read(handle->node, buffer, handle->offset, arg2);
    return ret;
}

syscall_(waitpid) {
    int      pid     = arg0;
    int     *status  = (int *)arg1;
    uint64_t options = arg2;
    if (pid == -1) goto wait;
    if (found_pcb(pid) == NULL) return SYSCALL_FAULT_(ECHILD);
wait:
    int ret_pid;
    int status0 = waitpid(pid, &ret_pid);
    if (status) *status = status0;
    return ret_pid;
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

    if (fd > 2) {
        fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
        if (!handle) return SYSCALL_FAULT_(EBADF);
        vfs_map(handle->node, addr, aligned_len, prot, flags, offset);
        return addr;
    }

    if (addr == 0 || (addr > USER_MMAP_START && addr < USER_MMAP_END)) {
        if (flags & MAP_FIXED) { return SYSCALL_FAULT_(EINVAL); }
        addr                 = process->mmap_start;
        flags               &= (~MAP_FIXED);
        process->mmap_start += aligned_len;
        if (process->mmap_start > USER_MMAP_END) {
            process->mmap_start -= aligned_len;
            return SYSCALL_FAULT_(ENOMEM);
        }
    }

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

syscall_(stat) {
    char        *fn  = (char *)arg0;
    struct stat *buf = (struct stat *)arg1;
    if (fn == NULL || buf == NULL) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = vfs_open(fn);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    buf->st_gid   = (int)node->group;
    buf->st_uid   = (int)node->owner;
    buf->st_size  = (long long int)node->size;
    buf->st_mode  = node->type | (node->type == file_symlink  ? S_IFLNK
                                  : node->type == file_dir    ? S_IFDIR
                                  : node->type == file_block  ? S_IFBLK
                                  : node->type == file_socket ? S_IFSOCK
                                  : node->type == file_none   ? S_IFREG
                                  : node->type == file_stream ? S_IFCHR
                                                              : 0);
    buf->st_nlink = 1;
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
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    int             ret    = vfs_ioctl(handle->node, options, (void *)arg2);
    return ret;
}

syscall_(writev) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return -(uint64_t)ENOSTR;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    int             iovcnt = arg2;
    struct iovec   *iov    = (struct iovec *)arg1;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    ssize_t         total  = 0;
    for (int i = 0; i < iovcnt; i++) {
        int status = vfs_write(handle->node, iov[i].iov_base, handle->offset, iov[i].iov_len);
        if (status == VFS_STATUS_FAILED) return total;
        total += iov[i].iov_len;
    }
    return total;
}

syscall_(readv) {
    int fd = (int)arg0;
    if (fd < 0 || arg1 == 0) return ENODEV;
    if (arg2 == 0) return SYSCALL_SUCCESS;
    size_t          iovcnt  = arg2;
    struct iovec   *iov     = (struct iovec *)arg1;
    fd_file_handle *handle  = queue_get(get_current_task()->parent_group->file_open, fd);
    size_t          buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf    = (uint8_t *)malloc(buf_len);
    size_t   status = vfs_read(handle->node, buf, handle->offset, buf_len);
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
    uint64_t vaddr  = arg0;
    size_t   length = arg1;
    if (length == 0) return SYSCALL_SUCCESS;
    unmap_page_range(get_current_directory(), vaddr, length);
    return SYSCALL_SUCCESS;
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
    size_t         timeout  = arg2;

    int      ready      = 0;
    uint64_t start_time = nano_time();
    bool     sigexit    = false;

    extern vfs_callback_t fs_callbacks[256];

    do {
        // 检查每个文件描述符
        size_t i = 0;
        queue_foreach(get_current_task()->parent_group->file_open, node0) {
            if (i > nfds) break;
            fd_file_handle *handle = (fd_file_handle *)node0->data;
            vfs_node_t      node   = handle->node;
            if (fs_callbacks[node->fsid]->poll == (void *)empty) {
                if (fds_user[i].events & POLLIN || fds_user[i].events & POLLOUT) {
                    fds_user[i].revents = fds_user[i].events & POLLIN ? POLLIN : POLLOUT;
                    ready++;
                }
                i++;
                continue;
            }
            int revents =
                epoll_to_poll_comp(vfs_poll(node, poll_to_epoll_comp(fds_user[i].events)));
            if (revents > 0) {
                fds_user[i].revents = revents;
                ready++;
            }
            i++;
        }

        // sigexit = signals_pending_quick(current_task);

        if (ready > 0 || sigexit) break;

        open_interrupt;
        __asm__("pause");
    } while (timeout != 0 && ((int)timeout == -1 || (nano_time() - start_time) < timeout));

    close_interrupt;

    if (!ready && sigexit) return (size_t)-EINTR;
    return ready;
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
    int       how    = (int)arg0;
    sigset_t *set    = (sigset_t *)arg1;
    sigset_t *oldset = (sigset_t *)arg2;
    return syscall_ssetmask(how, set, oldset);
}

syscall_(dup2) {
    int             fd     = (int)arg0;
    int             newfd  = (int)arg1;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);

    vfs_node_t new = vfs_dup(handle->node);
    if (!new) return SYSCALL_FAULT_(ENOSPC);
    fd_file_handle *new_node = queue_get(get_current_task()->parent_group->file_open, newfd);
    if (new_node != NULL) { vfs_close(new_node->node); }
    queue_enqueue_id(get_current_task()->parent_group->file_open, new, newfd);
    return newfd;
}

syscall_(dup) {
    int fd = (int)arg0;
    if (fd < 0) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    vfs_node_t new             = vfs_dup(handle->node);
    fd_file_handle *new_handle = malloc(sizeof(fd_file_handle));
    new_handle->offset         = 0;
    new_handle->node           = new;
    return queue_enqueue(get_current_task()->parent_group->file_open, new_handle);
}

syscall_(fcntl) {
    int      fd  = (int)arg0;
    int      cmd = (int)arg1;
    uint64_t arg = arg2;
    if (fd < 0 || cmd < 0) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    switch (cmd) {
    case F_GETFD: return !!(handle->node->flags & O_CLOEXEC);
    case F_SETFD: return handle->node->flags |= O_CLOEXEC;
    case F_DUPFD_CLOEXEC:
        uint64_t newfd       = syscall_dup(fd, 0, 0, 0, 0, 0, regs);
        handle->node->flags |= O_CLOEXEC;
        return newfd;
    case F_DUPFD: return syscall_dup(fd, 0, 0, 0, 0, 0, regs);
    case F_GETFL: return handle->node->flags;
    case F_SETFL:
        uint32_t valid_flags  = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        handle->node->flags  &= ~valid_flags;
        handle->node->flags  |= arg & valid_flags;
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
    return syscall_sig_action(sig, act, oldact);
}

syscall_(fork) {
    return process_fork(regs, false);
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

    return EOK; //TODO mprotect 实现有问题

    addr              = PADDING_DOWN(addr, PAGE_SIZE);
    size_t page_count = PADDING_UP(length, PAGE_SIZE) / PAGE_SIZE;
    pcb_t  process    = get_current_task()->parent_group;
    logkf("addr: %p, length: %zu, prot: %lx pc: %d\n", (void *)addr, length, prot, page_count);
    for (size_t i = 0; i < page_count; i++) {
        uint64_t page_addr = addr + i * PAGE_SIZE;

        bool updated = false;

        spin_lock(process->virt_queue->lock);
        queue_foreach(process->virt_queue, node) {
            mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
            if (page_addr >= vpage->start && page_addr < vpage->start + vpage->count * PAGE_SIZE) {
                uint64_t new_flags = vpage->pte_flags;
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
            logkf("physical page address: %p\n", (void *)page_addr);
            uint64_t old_flags;
            if (!page_table_get_flags(get_current_directory(), page_addr, &old_flags)) {
                return SYSCALL_FAULT_(ENOMEM);
            }
            logkf("old flags: %lx\n", old_flags);
            uint64_t new_flags = old_flags & ~(PTE_WRITEABLE | PTE_USER);
            if (prot & PROT_READ) new_flags |= PTE_PRESENT;
            if (prot & PROT_WRITE) new_flags |= PTE_WRITEABLE;
            if (prot & PROT_EXEC) new_flags |= PTE_USER;
            page_table_update_flags(get_current_directory(), page_addr, new_flags);
        }
    }

    return SYSCALL_SUCCESS;
}

syscall_(vfork) {
    return process_fork(regs, true);
}

syscall_(execve) {
    char  *path = (char *)arg0;
    char **argv = (char **)arg1;
    char **envp = (char **)arg2;
    if (path == NULL) return SYSCALL_FAULT_(EINVAL);
    return process_execve(path, argv, envp);
}

syscall_(fstat) {
    int          fd  = (int)arg0;
    struct stat *buf = (struct stat *)arg1;
    if (buf == NULL) return SYSCALL_FAULT_(EINVAL);

    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    vfs_node_t node = handle->node;
    buf->st_gid     = (int)node->group;
    buf->st_uid     = (int)node->owner;
    buf->st_size    = (long long int)node->size;
    buf->st_mode    = node->type | (node->type == file_symlink  ? S_IFLNK
                                    : node->type == file_dir    ? S_IFDIR
                                    : node->type == file_block  ? S_IFBLK
                                    : node->type == file_socket ? S_IFSOCK
                                    : node->type == file_none   ? S_IFREG
                                    : node->type == file_stream ? S_IFCHR
                                                                : 0);
    buf->st_nlink   = 1;
    return node->size;
}

syscall_(clock_gettime) {
    switch (arg0) {
    case 1:
    case 6:
    case 4: {
        if (arg1 != 0) {
            struct timespec *ts   = (struct timespec *)arg1;
            uint64_t         nano = nano_time();
            ts->tv_sec            = nano / 1000000000ULL;
            ts->tv_nsec           = nano % 1000000000ULL;
        }
        return SYSCALL_SUCCESS;
    }
    case 0: {
        uint64_t timestamp = mktime();
        if (arg1 != 0) {
            struct timespec *ts = (struct timespec *)arg1;
            ts->tv_sec          = timestamp;
            ts->tv_nsec         = 0;
        }
        return SYSCALL_SUCCESS;
    }
    default: logkf("clock not supported\n"); return SYSCALL_FAULT_(EINVAL);
    }
}

syscall_(clock_getres) {
    if (arg2 == 0) return SYSCALL_FAULT_(EINVAL);
    ((struct timespec *)arg2)->tv_nsec = 1000000;
    return SYSCALL_SUCCESS;
}

syscall_(sigsuspend) {
    sigset_t *mask = (sigset_t *)arg0;
    if (mask == NULL) return SYSCALL_FAULT_(EINVAL);
    sigset_t old = get_current_task()->blocked;

    get_current_task()->blocked = *mask;
    while (!signals_pending_quick(get_current_task())) {
        open_interrupt;
        __asm__("pause");
    }
    close_interrupt;
    get_current_task()->blocked = old;
    return SYSCALL_FAULT_(EINTR);
}

syscall_(mkdir) {
    char    *name = (char *)arg0;
    uint64_t mode = arg1;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    return vfs_mkdir(name) == VFS_STATUS_FAILED ? -1 : 0;
}

syscall_(lseek) {
    int             fd     = (int)arg0;
    size_t          offset = arg1;
    size_t          whence = arg2;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    int64_t real_offset = offset;
    if (real_offset < 0 && handle->node->type & file_none && whence != SEEK_CUR)
        return SYSCALL_FAULT_(EBADF);
    switch (whence) {
    case SEEK_SET: handle->offset = real_offset; break;
    case SEEK_CUR:
        handle->offset += real_offset;
        if ((int64_t)handle->offset < 0) {
            handle->offset = 0;
        } else if (handle->offset > handle->node->size) {
            handle->offset = handle->node->size;
        }
        break;
    case SEEK_END: handle->offset = handle->node->size - real_offset; break;
    default: return SYSCALL_FAULT_(ENOSYS);
    }

    return handle->offset;
}

syscall_(select) {
    int             nfds    = (int)arg0;
    uint8_t        *read    = (uint8_t *)arg1;
    uint8_t        *write   = (uint8_t *)arg2;
    uint8_t        *except  = (uint8_t *)arg3;
    struct timeval *timeout = (struct timeval *)arg4;
    if (read && check_user_overflow((uint64_t)read, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (write && check_user_overflow((uint64_t)write, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (except && check_user_overflow((uint64_t)except, sizeof(struct pollfd))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    size_t         complength = sizeof(struct pollfd);
    struct pollfd *comp       = (struct pollfd *)malloc(complength);
    size_t         compIndex  = 0;
    if (read) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(read, i)) select_add(&comp, &compIndex, &complength, i, POLLIN);
        }
    }
    if (write) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(write, i)) select_add(&comp, &compIndex, &complength, i, POLLOUT);
        }
    }
    if (except) {
        for (int i = 0; i < nfds; i++) {
            if (select_bitmap(except, i))
                select_add(&comp, &compIndex, &complength, i, POLLPRI | POLLERR);
        }
    }

    int toZero = (nfds + 8) / 8;
    if (read) memset(read, 0, toZero);
    if (write) memset(write, 0, toZero);
    if (except) memset(except, 0, toZero);

    size_t res = syscall_poll(
        (uint64_t)comp, compIndex,
        timeout ? (timeout->tv_sec * 1000 + (timeout->tv_usec + 1000) / 1000) : -1, 0, 0, 0, 0);

    if ((int64_t)res < 0) {
        free(comp);
        return res;
    }

    size_t verify = 0;
    for (size_t i = 0; i < compIndex; i++) {
        if (!comp[i].revents) continue;
        if (comp[i].events & POLLIN && comp[i].revents & POLLIN) {
            select_bitmap_set(read, comp[i].fd);
            verify++;
        }
        if (comp[i].events & POLLOUT && comp[i].revents & POLLOUT) {
            select_bitmap_set(write, comp[i].fd);
            verify++;
        }
        if ((comp[i].events & POLLPRI && comp[i].revents & POLLPRI) ||
            (comp[i].events & POLLERR && comp[i].revents & POLLERR)) {
            select_bitmap_set(except, comp[i].fd);
            verify++;
        }
    }

    free(comp);
    return verify;
}

syscall_(pselect6) {
    uint64_t         nfds          = arg0;
    fd_set          *readfds       = (fd_set *)arg1;
    fd_set          *writefds      = (fd_set *)arg2;
    fd_set          *exceptfds     = (fd_set *)arg3;
    struct timespec *timeout       = (struct timespec *)arg4;
    WeirdPselect6   *weirdPselect6 = (WeirdPselect6 *)arg5;
    if (readfds && check_user_overflow((uint64_t)readfds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (writefds && check_user_overflow((uint64_t)writefds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (exceptfds && check_user_overflow((uint64_t)exceptfds, sizeof(fd_set) * nfds)) {
        return SYSCALL_FAULT_(EFAULT);
    }
    size_t    sigsetsize = weirdPselect6->ss_len;
    sigset_t *sigmask    = weirdPselect6->ss;
    if (sigsetsize < sizeof(sigset_t)) { return SYSCALL_FAULT_(EINVAL); }
    sigset_t origmask;
    if (sigmask) syscall_ssetmask(SIG_SETMASK, sigmask, &origmask);
    struct timeval timeoutConv;
    if (timeout) {
        timeoutConv = (struct timeval){.tv_sec  = timeout->tv_sec,
                                       .tv_usec = (timeout->tv_nsec + 1000) / 1000};
    } else {
        timeoutConv = (struct timeval){.tv_sec = (uint64_t)-1, .tv_usec = (uint64_t)-1};
    }

    size_t ret = syscall_select(nfds, (uint64_t)(uint8_t *)readfds, (uint64_t)(uint8_t *)writefds,
                                (uint64_t)(uint8_t *)exceptfds, (uint64_t)&timeoutConv, 0, 0);
    if (sigmask) syscall_ssetmask(SIG_SETMASK, &origmask, NULL);
    return ret;
}

syscall_(reboot) {
    int      magic1 = arg0;
    int      magic2 = arg1;
    uint32_t cmd    = arg2;
    void    *arg    = (void *)arg3;

    extern void cp_shutdown();
    extern void cp_reset();

    if (magic1 != REBOOT_MAGIC1 || magic2 != REBOOT_MAGIC2) return SYSCALL_FAULT_(EINVAL);
    switch (cmd) {
    case REBOOT_CMD_RESTART: cp_reset(); break;
    case REBOOT_CMD_CAD_OFF: cp_shutdown(); break;
    default: return SYSCALL_FAULT_(EINVAL);
    }
    return SYSCALL_SUCCESS;
}

syscall_(getdents) {
    int      fd   = arg0;
    uint64_t buf  = arg1;
    size_t   size = arg2;
    if (check_user_overflow(buf, size)) { return SYSCALL_FAULT_(EFAULT); }
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }
    size_t         child_count   = (uint64_t)list_length(handle->node->child);
    struct dirent *dents         = (struct dirent *)buf;
    int64_t        max_dents_num = size / sizeof(struct dirent);
    int64_t        read_count    = 0;
    uint64_t       offset        = 0;
    list_foreach(handle->node->child, i) {
        if (offset < handle->offset) goto next;
        if (handle->offset >= (child_count * sizeof(struct dirent))) break;
        if (read_count >= max_dents_num) break;
        vfs_node_t child_node      = (vfs_node_t)i->data;
        dents[read_count].d_ino    = 0;
        dents[read_count].d_off    = handle->offset;
        dents[read_count].d_reclen = sizeof(struct dirent);
        if (child_node->type & file_symlink)
            dents[read_count].d_type = DT_LNK;
        else if (child_node->type & file_none)
            dents[read_count].d_type = DT_REG;
        else if (child_node->type & file_dir)
            dents[read_count].d_type = DT_DIR;
        else
            dents[read_count].d_type = DT_UNKNOWN;
        strncpy(dents[read_count].d_name, child_node->name, 1024);
        handle->offset += sizeof(struct dirent);
        read_count++;
    next:
        offset += sizeof(struct dirent);
    }
    return read_count * sizeof(struct dirent);
}

syscall_(newfstatat) {
    int          dirfd    = arg0;
    char        *pathname = (char *)arg1;
    struct stat *buf      = (struct stat *)arg2;
    uint64_t     flags    = arg3;

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);

    uint64_t ret = syscall_stat((uint64_t)resolved, (uint64_t)buf, 0, 0, 0, 0, 0);

    free(resolved);

    return ret;
}

syscall_(statx) {
    int           dirfd    = arg0;
    char         *pathname = (char *)arg1;
    uint64_t      flags    = arg2;
    uint64_t      mask     = arg3;
    struct statx *buff     = (struct statx *)arg4;
    if (!pathname || check_user_overflow((uint64_t)pathname, strlen(pathname))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (!buff || check_user_overflow((uint64_t)buff, sizeof(struct statx))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    struct stat simple;
    memset(&simple, 0, sizeof(struct stat));
    uint64_t ret = syscall_newfstatat(dirfd, (uint64_t)pathname, (uint64_t)&simple, flags, 0, 0, 0);
    if ((int64_t)ret < 0) return ret;

    buff->stx_mask            = mask;
    buff->stx_blksize         = simple.st_blksize;
    buff->stx_attributes      = 0;
    buff->stx_nlink           = simple.st_nlink;
    buff->stx_uid             = simple.st_uid;
    buff->stx_gid             = simple.st_gid;
    buff->stx_mode            = simple.st_mode;
    buff->stx_ino             = simple.st_ino;
    buff->stx_size            = simple.st_size;
    buff->stx_blocks          = simple.st_blocks;
    buff->stx_attributes_mask = 0;

    buff->stx_atime.tv_sec  = simple.st_atim.tv_sec;
    buff->stx_atime.tv_nsec = simple.st_atim.tv_nsec;

    buff->stx_btime.tv_sec  = simple.st_ctim.tv_sec;
    buff->stx_btime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_ctime.tv_sec  = simple.st_ctim.tv_sec;
    buff->stx_ctime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_mtime.tv_sec  = simple.st_mtim.tv_sec;
    buff->stx_mtime.tv_nsec = simple.st_mtim.tv_nsec;
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
    [SYSCALL_STAT]        = syscall_stat,
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
    [SYSCALL_VFORK]       = syscall_vfork,
    [SYSCALL_EXECVE]      = syscall_execve,
    [SYSCALL_FSTAT]       = syscall_fstat,
    [SYSCALL_C_GETTIME]   = syscall_clock_gettime,
    [SYSCALL_C_GETRES]    = syscall_clock_getres,
    [SYSCALL_SIGSUSPEND]  = syscall_sigsuspend,
    [SYSCALL_MKDIR]       = syscall_mkdir,
    [SYSCALL_LSEEK]       = syscall_lseek,
    [SYSCALL_PSELECT6]    = syscall_pselect6,
    [SYSCALL_SELECT]      = syscall_select,
    [SYSCALL_REBOOT]      = syscall_reboot,
    [SYSCALL_GETDENTS64]  = syscall_getdents,
    [SYSCALL_STATX]       = syscall_statx,
    [SYSCALL_NEWFSTATAT]  = syscall_newfstatat,
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

    tcb_t thread = get_current_task();
    write_fsbase((uint64_t)thread);
    thread->context0.rsp    = regs->rsp;
    thread->context0.rip    = regs->rip;
    thread->context0.rflags = regs->rflags;
    thread->context0.cs     = regs->cs;
    thread->context0.ss     = regs->ss;
    thread->context0.ds     = regs->ds;
    thread->context0.es     = regs->es;
    thread->context0.rdi    = regs->rdi;
    thread->context0.rsi    = regs->rsi;
    thread->context0.rdx    = regs->rdx;
    thread->context0.r10    = regs->r10;
    thread->context0.r8     = regs->r8;
    thread->context0.r9     = regs->r9;
    thread->context0.r15    = regs->r15;
    thread->context0.r14    = regs->r14;
    thread->context0.r13    = regs->r13;
    thread->context0.r12    = regs->r12;
    thread->context0.r11    = regs->r11;
    thread->context0.rbx    = regs->rbx;
    thread->context0.rcx    = regs->rcx;
    thread->context0.rbp    = regs->rbp;

    uint64_t syscall_id = regs->rax & 0xFFFFFFFF;
    logkf("syscall start: %d P(%s) id:%d\n", syscall_id, thread->name, thread->pid);
    if (syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL) {
        regs->rax = ((syscall_t)syscall_handlers[syscall_id])(regs->rdi, regs->rsi, regs->rdx,
                                                              regs->r10, regs->r8, regs->r9, regs);
    } else
        regs->rax = SYSCALL_FAULT;
    logkf("SYScall: %d RET:%d\n", syscall_id, regs->rax);
    write_fsbase(thread->fs_base);
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
