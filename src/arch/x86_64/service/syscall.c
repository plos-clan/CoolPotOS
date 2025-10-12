#define ALL_IMPLEMENTATION

#include "syscall.h"
#include "boot.h"
#include "cpuid.h"
#include "cpustats.h"
#include "errno.h"
#include "fsgsbase.h"
#include "heap.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lazyalloc.h"
#include "lock_queue.h"
#include "memstats.h"
#include "network.h"
#include "page.h"
#include "pcb.h"
#include "pipefs.h"
#include "scheduler.h"
#include "signal.h"
#include "sprintf.h"
#include "time.h"
#include "timer.h"
#include "vfs.h"
#include "cow_arraylist.h"

spin_t mm_op_lock = SPIN_INIT;

extern cow_arraylist *pgb_queue;

USED uint64_t get_kernel_stack() {
    return get_current_task()->kernel_stack;
}

__attribute__((naked)) void asm_syscall_handle() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "cld\n\t"
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov [rax+0x08], rsp\n\t"
                     "cmp QWORD PTR [rax+0x18], 0\n\t"
                     "je normal\n\t"
                     "signal:\n\t" //信号处理
                     "mov rsp, [rax+0x10]\n\t"
                     "jmp next\n\t"
                     "normal:\n\t" // 默认处理
                     "mov rsp, [rax+0x0] \n\t"
                     "jmp next\n\t"
                     "next:\n\t"
                     "sub rsp, 0x38\n\t"
                     "mov rax, cr2\n\t"
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
                     "mov rdi, rsp\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsi, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "call syscall_handler\n\t"
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
                     "swapgs\n\t"
                     "mov cr2, rax\n\t"
                     "mov rax, QWORD PTR gs:0x00\n\t"
                     "mov rsp, [rax+0x8]\n\t"
                     "mov rax, cr2\n\t"
                     "swapgs\n\t"
                     "sysretq\n\t" ::
                         : "memory");
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

syscall_(exit, int exit_code) {
    tcb_t exit_thread = get_current_task();
    logkf("sys_exit: Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    kill_thread(exit_thread);
    open_interrupt;
    cpu_hlt;
    return SYSCALL_SUCCESS;
}

syscall_(open, char *path0, uint64_t flags, uint64_t mode) {
    if (unlikely(path0 == NULL)) return SYSCALL_FAULT_(EINVAL);

    char *normalized_path = vfs_cwd_path_build(path0);

    logkf("sys_open: open %s\n", normalized_path);

    vfs_node_t node = vfs_open(normalized_path);
    if (node == NULL) {
        if (flags & O_CREAT) {
            if (mode & O_DIRECTORY) {
                vfs_mkdir(normalized_path);
            } else
                vfs_mkfile(normalized_path);
            node = vfs_open(normalized_path);
            if (node == NULL)
                goto err;
            else
                goto next;
        } else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
next:
    fd_file_handle *fd_handle = calloc(1, sizeof(fd_file_handle));
    not_null_assets(fd_handle, "sys_open: null alloc fd");
    fd_handle->offset = flags & O_APPEND ? node->size : 0;
    fd_handle->node   = node;
    int index     = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);
    fd_handle->fd = index;
    spin_unlock(get_current_task()->parent_group->file_open->lock);
    if (index == -1) {
        logkf("sys_open: open %s failed.\n", normalized_path);
        vfs_close(node);
        free(fd_handle);
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(normalized_path);
    return index;
}

syscall_(close, int fd) {
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =
        (fd_file_handle *)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_close(handle->node);
    free(handle);
    return SYSCALL_SUCCESS;
}

syscall_(write, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return SYSCALL_SUCCESS;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    size_t ret = vfs_write(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)VFS_STATUS_FAILED) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) handle->offset += ret;
    vfs_update(handle->node);
    return ret;
}

syscall_(read, int fd, uint8_t *buffer, size_t size) {
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return SYSCALL_SUCCESS;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    if (handle->node->size != (uint64_t)-1) {
        if (handle->offset >= handle->node->size) return EOK;
    }
    size_t ret = vfs_read(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)VFS_STATUS_FAILED) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) { handle->offset += ret; }
    return ret;
}

syscall_(waitpid, pid_t pid, int *status, uint64_t options) {
    if (get_current_task()->parent_group->child_pcb->size == 0) return SYSCALL_FAULT_(ECHILD);
    if (pid == -1) goto wait;
    pcb_t wait_p = found_pcb(pid);
    if (wait_p == NULL) return SYSCALL_FAULT_(ECHILD);
wait:
    pid_t ret_pid;
    int   status0 = waitpid(pid, &ret_pid);
    if (status) *status = status0;
    return ret_pid;
}

syscall_(mmap, uint64_t addr, size_t length, uint64_t prot, uint64_t flags, int fd,
         uint64_t offset) {

    addr = addr & (~(PAGE_SIZE - 1));

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    if (check_user_overflow(addr, aligned_len)) { return -EFAULT; }

    if (aligned_len == 0) { return SYSCALL_FAULT_(EINVAL); }
    pcb_t process = get_current_task()->parent_group;

    vma_manager_t *mgr = &process->vma_manager;
    uint64_t       start_addr;
    if (flags & MAP_FIXED) {
        if (!addr) return SYSCALL_FAULT_(EINVAL);

        start_addr = addr;
        // 检查地址是否可用
        if (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
            vma_unmap_range(mgr, start_addr, start_addr + aligned_len);
        }
    } else {
        if (addr) {
            start_addr = addr;
            // 检查地址是否可用
            if (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
                return SYSCALL_FAULT_(ENOMEM);
            }
        } else {
            start_addr = USER_MMAP_START;
            while (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
                start_addr += PAGE_SIZE;
                if (start_addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(ENOMEM);
            }
        }
    }

    if (!(flags & MAP_ANONYMOUS)) {
        if (queue_get(process->file_open, fd) == NULL) return SYSCALL_FAULT_(EBADF);
    }

    spin_lock(mm_op_lock);

    vma_t *vma = vma_alloc();
    if (!vma) return SYSCALL_FAULT_(ENOMEM);

    vma->vm_start = start_addr;
    vma->vm_end   = start_addr + aligned_len;
    vma->vm_flags = 0;

    if (prot & PROT_READ) vma->vm_flags |= VMA_READ;
    if (prot & PROT_WRITE) vma->vm_flags |= VMA_WRITE;
    if (prot & PROT_EXEC) vma->vm_flags |= VMA_EXEC;
    if (flags & MAP_SHARED) vma->vm_flags |= VMA_SHARED;

    if (flags & MAP_ANONYMOUS) {
        vma->vm_type   = VMA_TYPE_ANON;
        vma->vm_flags |= VMA_ANON;
        vma->vm_fd     = -1;
    } else {
        vma->vm_type   = VMA_TYPE_FILE;
        vma->vm_fd     = fd;
        vma->vm_offset = offset;
    }

    vma_t *region = vma_find_intersection(mgr, start_addr, start_addr + aligned_len);
    if (region) {
        vma_remove(mgr, region);
        vma_free(region);
    }

    if (vma_insert(mgr, vma) != 0) {
        vma_free(vma);
        return SYSCALL_FAULT_(ENOMEM);
    }

    if (!(flags & MAP_ANONYMOUS)) {
        uint64_t ret = (uint64_t)vfs_map(queue_get(process->file_open, fd), start_addr, aligned_len,
                                         prot, flags, offset);

        spin_unlock(mm_op_lock);
        return ret;
    } else {
        uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

        if (prot != PROT_NONE) {
            if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
            if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
            if (!(prot & PROT_EXEC)) pt_flags |= PTE_NO_EXECUTE;
        }

        lazy_infoalloc(process, start_addr, aligned_len, pt_flags, flags);
        spin_unlock(mm_op_lock);

        return start_addr;
    }

    return addr;
}

syscall_(signal, int sig) {
    if (sig < 0 || sig >= MAX_SIGNALS) return SYSCALL_FAULT_(EINVAL);
    void *handler = (void *)arg1;
    if (handler == NULL) return SYSCALL_FAULT_(EINVAL);
    logkf("Signal syscall: %p\n", handler);
    register_signal(get_current_task()->parent_group, sig, handler);
    return SYSCALL_SUCCESS;
}

syscall_(sigret) {}

syscall_(getpid) {
    if (unlikely(arg0 == UINT64_MAX || arg1 == UINT64_MAX || arg2 == UINT64_MAX ||
                 arg3 == UINT64_MAX || arg4 == UINT64_MAX))
        return 1;
    return get_current_task()->parent_group->pid;
}

syscall_(prctl) {
    return process_control(arg0, arg1, arg2, arg3, arg4);
}

syscall_(stat, char *fn, struct stat *buf) {
    if (unlikely(fn == NULL || buf == NULL)) return SYSCALL_FAULT_(EINVAL);
    char      *path = vfs_cwd_path_build(fn);
    vfs_node_t node = vfs_open(path);
    if (node == NULL) {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    buf->st_gid   = (int)node->group;
    buf->st_uid   = (int)node->owner;
    buf->st_ino   = node->inode;
    buf->st_size  = (long long int)node->size;
    buf->st_mode  = node->mode | (node->type == file_symlink  ? S_IFLNK
                                  : node->type == file_dir    ? S_IFDIR
                                  : node->type == file_block  ? S_IFBLK
                                  : node->type == file_socket ? S_IFSOCK
                                  : node->type == file_none   ? S_IFREG
                                  : node->type == file_stream ? S_IFCHR
                                                              : 0);
    buf->st_nlink = 1;
    buf->st_dev   = node->dev;
    buf->st_rdev  = node->rdev;
    free(path);
    return SYSCALL_SUCCESS;
}

syscall_(clone, uint64_t flags, uint64_t stack, int *parent_tid, int *child_tid, uint64_t tls) {
    close_interrupt;
    disable_scheduler();
    uint64_t id = thread_clone(regs, flags, stack, parent_tid, child_tid, tls);
    open_interrupt;
    enable_scheduler();
    return id;
}

syscall_(arch_prctl, uint64_t code, uint64_t addr) {
    tcb_t thread = get_current_task();
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

syscall_(uname, struct utsname *utsname) {
    if (unlikely(utsname == NULL)) return SYSCALL_FAULT_(EINVAL);
    char sysname[] = "CoolPotOS";
    char machine[] = "x86_64";
    char version[] = "0.0.1";
    memcpy(utsname->sysname, sysname, sizeof(sysname));
    memcpy(utsname->nodename, "localhost", 50);
    memcpy(utsname->release, KERNEL_NAME, sizeof(KERNEL_NAME));
    memcpy(utsname->version, version, sizeof(version));
    memcpy(utsname->machine, machine, sizeof(machine));
    return SYSCALL_SUCCESS;
}

syscall_(nano_sleep, void *time_handle) {
    struct timespec k_req;
    if (unlikely(time_handle == NULL)) return SYSCALL_FAULT_(EINVAL);
    memcpy(&k_req, time_handle, sizeof(k_req));
    if (unlikely(k_req.tv_nsec >= 1000000000L)) return SYSCALL_FAULT_(EINVAL);
    uint64_t nsec = (uint64_t)k_req.tv_sec * 1000000000ULL + k_req.tv_nsec;
    scheduler_nano_sleep(nsec);
    return SYSCALL_SUCCESS;
}

syscall_(ioctl, int fd, int options) {
    if (unlikely(fd < 0 || arg2 == 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    int             ret    = vfs_ioctl(handle->node, options, (void *)arg2);
    return ret;
}

syscall_(writev, int fd, struct iovec *iov, int iovcnt) {
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt == 0) return SYSCALL_SUCCESS;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    ssize_t         total  = 0;
    for (int i = 0; i < iovcnt; i++) {
        int status = vfs_write(handle->node, iov[i].iov_base, handle->offset, iov[i].iov_len);
        if (handle->node->size != (uint64_t)-1) {
            if (status == VFS_STATUS_FAILED) return total;
            handle->offset += status;
        }
        total += iov[i].iov_len;
    }
    return total;
}

syscall_(readv, int fd, struct iovec *iov, int iovcnt0) {
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt0 == 0) return SYSCALL_SUCCESS;
    size_t          iovcnt  = iovcnt0;
    fd_file_handle *handle  = queue_get(get_current_task()->parent_group->file_open, fd);
    size_t          buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++) {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf = (uint8_t *)malloc(buf_len);
    if (handle->node->size != (uint64_t)-1) {
        if (handle->offset > handle->node->size) return SYSCALL_SUCCESS;
    }
    size_t status = vfs_read(handle->node, buf, handle->offset, buf_len);
    if (status == (size_t)VFS_STATUS_FAILED) {
        free(buf);
        return SYSCALL_FAULT_(EIO);
    }
    if (handle->node->size != (uint64_t)-1) handle->offset += status;
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

syscall_(munmap, uint64_t addr, size_t size) {
    if (size == 0) return SYSCALL_SUCCESS;

    addr = addr & (~(PAGE_SIZE - 1));
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (check_user_overflow(addr, size)) { return -EFAULT; }

    vma_manager_t *mgr = &get_current_task()->parent_group->vma_manager;
    vma_t         *vma = mgr->vma_list;
    vma_t         *next;

    uint64_t start = addr;
    uint64_t end   = addr + size;

    while (vma) {
        next = vma->vm_next;

        // 完全包含在要取消映射的范围内
        if (vma->vm_start >= start && vma->vm_end <= end) {
            vma_remove(mgr, vma);
            vma_free(vma);
        }
        // 部分重叠 - 需要分割
        else if (!(vma->vm_end <= start || vma->vm_start >= end)) {
            if (vma->vm_start < start && vma->vm_end > end) {
                // VMA跨越整个取消映射范围 - 分割成两部分
                vma_split(vma, end);
                vma_split(vma, start);
                // 移除中间部分
                vma_t *middle = vma->vm_next;
                vma_remove(mgr, middle);
                vma_free(middle);
            } else if (vma->vm_start < start) {
                // 截断VMA的末尾
                vma->vm_end = start;
            } else if (vma->vm_end > end) {
                // 截断VMA的开头
                vma->vm_start = end;
                if (vma->vm_type == VMA_TYPE_FILE) { vma->vm_offset += end - vma->vm_start; }
            }
        }

        vma = next;
    }

    unmap_virtual_page(get_current_task()->parent_group, addr, size);
    unmap_page_range(get_current_directory(), addr, size);
    return SYSCALL_SUCCESS;
}

syscall_(mremap, uint64_t old_addr, uint64_t old_size, uint64_t new_size, uint64_t flags,
         uint64_t new_addr) {
    old_addr = old_addr & (~(PAGE_SIZE - 1));
    new_addr = new_addr & (~(PAGE_SIZE - 1));
    old_size = (old_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    new_size = (new_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    vma_manager_t *mgr = &get_current_task()->parent_group->vma_manager;

    vma_t *vma = vma_find(mgr, (unsigned long)old_addr);
    if (!vma || vma->vm_start != (unsigned long)old_addr) { return SYSCALL_FAULT_(EINVAL); }

    uint64_t old_addr_phys = page_virt_to_phys(old_addr);

    // 如果新大小更小，直接截断
    if (new_size <= vma->vm_end - vma->vm_start) {
        unmap_page_range(get_current_directory(), vma->vm_end,
                         vma->vm_start + new_size - vma->vm_end);
        vma->vm_end = vma->vm_start + new_size;
        return old_addr;
    }

    // 如果需要扩大，检查是否有足够空间
    uint64_t new_end = vma->vm_start + new_size;
    if (!vma_find_intersection(mgr, vma->vm_end, new_end)) {
        uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

        if (vma->vm_flags & VMA_READ) pt_flags |= PTE_PRESENT;
        if (vma->vm_flags & VMA_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(vma->vm_flags & VMA_EXEC)) pt_flags |= PTE_NO_EXECUTE;

        page_map_range(get_current_directory(), vma->vm_end,
                       old_addr_phys + vma->vm_end - vma->vm_start, new_end - vma->vm_end,
                       vma->vm_flags);

        vma->vm_end = new_end;
        return old_addr;
    }

    if (flags & MREMAP_MAYMOVE) {
        // 简单的地址分配策略：从高地址开始
        uint64_t start_addr = USER_MMAP_START;
        while (vma_find_intersection(mgr, start_addr, start_addr + new_size)) {
            start_addr += PAGE_SIZE;
            if (start_addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(ENOMEM);
        }

        vma_t *new_vma = vma_alloc();
        if (!new_vma) return SYSCALL_FAULT_(ENOMEM);

        memcpy(new_vma, vma, sizeof(vma_t));
        new_vma->vm_start = start_addr;
        new_vma->vm_end   = start_addr + new_size;
        new_vma->vm_flags = 0;

        if (vma_insert(mgr, new_vma) != 0) {
            vma_free(new_vma);
            return SYSCALL_FAULT_(ENOMEM);
        }

        uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

        if (new_vma->vm_flags & VMA_READ) pt_flags |= PTE_PRESENT;
        if (new_vma->vm_flags & VMA_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(new_vma->vm_flags & VMA_EXEC)) pt_flags |= PTE_NO_EXECUTE;

        page_map_range(get_current_directory(), start_addr, old_addr_phys, new_size, pt_flags);

        syscall_munmap(old_addr, old_size, 0, 0, 0, 0, regs);
        return start_addr;
    }

    return (uint64_t)-ENOMEM;
}

syscall_(getcwd, char *buffer, size_t length) {
    if (unlikely(buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(length == 0)) return SYSCALL_SUCCESS;
    pcb_t  process  = get_current_task()->parent_group;
    char  *cwd      = vfs_get_fullpath(process->cwd);
    size_t cwd_leng = strlen(cwd);
    if (length > cwd_leng) length = cwd_leng;
    memcpy(buffer, cwd, length);
    return length;
}

syscall_(chdir, char *s) {
    if (unlikely(s == NULL)) return SYSCALL_FAULT_(EINVAL);
    pcb_t process = get_current_task()->parent_group;

    char *path;
    char *bpath = NULL;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        bpath = vfs_get_fullpath(process->cwd);
        path  = pathacat(bpath, s);
    }

    char *normalized_path = normalize_path(path);
    free(path);
    free(bpath);

    if (unlikely(normalized_path == NULL)) { return SYSCALL_FAULT_(ENOMEM); }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL) {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (node->type == file_dir) {
        process->cwd = node;
    } else {
        return SYSCALL_FAULT_(ENOTDIR);
    }

    free(normalized_path);
    return SYSCALL_SUCCESS;
}

syscall_(exit_group, int exit_code) {
    pcb_t exit_process = get_current_task()->parent_group;
    logkf("task: Process %s exit with code %d.\n", exit_process->name, exit_code);
    close_interrupt;
    kill_proc(exit_process, exit_code,
              true); // 子进程调用，is_zombie = true，不能在child_pcb中删除当前进程
    open_interrupt;
    //scheduler_yield();
    cpu_hlt;
}

syscall_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout) {
    int      ready      = 0;
    uint64_t start_time = nano_time();
    bool     sigexit    = false;

    extern vfs_callback_t fs_callbacks[256];

    do {
        // 检查每个文件描述符
        size_t i = 0;
        queue_foreach(get_current_task()->parent_group->file_open, node0) {
            if (i >= nfds) break;
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

syscall_(set_tid_address, int *tidptr) {
    if (unlikely(tidptr == NULL)) return SYSCALL_FAULT_(EINVAL);
    tcb_t thread          = get_current_task();
    thread->tid_address   = (uint64_t)tidptr;
    thread->tid_directory = get_current_directory();
    return SYSCALL_SUCCESS;
}

syscall_(sched_getaffinity) {
    pid_t  pid      = (int)arg0;
    size_t set_size = (size_t)arg1;
    return SYSCALL_FAULT_(ENOSYS);
}

syscall_(rt_sigprocmask, int how, sigset_t *set, sigset_t *oldset) {
    return syscall_ssetmask(how, set, oldset);
}

extern fd_file_handle *fd_dup(fd_file_handle *src);

syscall_(dup2, int fd, int newfd) {
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

    fd_file_handle *old_handle = queue_get(get_current_task()->parent_group->file_open, newfd);
    if (old_handle != NULL) {
        queue_remove_at(get_current_task()->parent_group->file_open, newfd);
        vfs_close(old_handle->node);
        free(old_handle);
    }

    fd_file_handle *new_handle = fd_dup(handle);
    if (new_handle == NULL) return SYSCALL_FAULT_(ENOMEM);
    queue_enqueue_id(get_current_task()->parent_group->file_open, new_handle, newfd);
    return newfd;
}

syscall_(dup, int fd) {
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    fd_file_handle *new_handle = fd_dup(handle);
    return new_handle->fd = queue_enqueue(get_current_task()->parent_group->file_open, new_handle);
}

syscall_(fcntl, int fd, int cmd, uint64_t arg) {
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

syscall_(sigaltstack, altstack_t *old_stack, altstack_t *new_stack) {
    tcb_t thread = get_current_task();
    if (old_stack) { *old_stack = thread->alt_stack; }
    if (new_stack) { thread->alt_stack = *new_stack; }
    return SYSCALL_SUCCESS;
}

syscall_(sigaction, int sig, struct sigaction *act, struct sigaction *oldact) {
    return syscall_sig_action(sig, act, oldact);
}

syscall_(fork) {
    return process_fork(regs, false, 0);
}

syscall_(futex, int *uaddr, int op, int val, struct timespec *time, int timeout) {
    tcb_t thread     = get_current_task();
    bool  is_pre_sub = false;
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
    return get_current_task()->tid;
}

syscall_(mprotect, uint64_t addr, size_t length, uint64_t prot) {
    addr   = addr & (~(PAGE_SIZE - 1));
    length = (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    if (check_user_overflow(addr, length)) { return SYSCALL_FAULT_(EFAULT); }

    uint64_t pt_flags = PTE_USER;

    if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
    if (prot & PROT_EXEC) pt_flags |= PTE_USER;

    map_change_attribute_range(get_current_directory(), addr & (~(PAGE_SIZE - 1)),
                               (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)), pt_flags);

    return EOK; //TODO mprotect 实现有问题
}

syscall_(vfork) {
    return process_fork(regs, true, 0);
}

syscall_(execve) {
    char  *path = (char *)arg0;
    char **argv = (char **)arg1;
    char **envp = (char **)arg2;
    if (unlikely(path == NULL)) return SYSCALL_FAULT_(EINVAL);
    return process_execve(path, argv, envp);
}

syscall_(fstat) {
    int          fd  = (int)arg0;
    struct stat *buf = (struct stat *)arg1;
    if (unlikely(buf == NULL)) return SYSCALL_FAULT_(EINVAL);

    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);
    vfs_node_t node = handle->node;
    buf->st_gid     = (int)node->group;
    buf->st_uid     = (int)node->owner;
    buf->st_size    = node->size == (uint64_t)-1 ? 0 : (long long int)node->size;
    buf->st_mode    = node->type | (node->type == file_symlink  ? S_IFLNK
                                    : node->type == file_dir    ? S_IFDIR
                                    : node->type == file_block  ? S_IFBLK
                                    : node->type == file_socket ? S_IFSOCK
                                    : node->type == file_none   ? S_IFREG
                                    : node->type == file_stream ? S_IFCHR
                                                                : 0);
    buf->st_nlink   = 1;
    buf->st_dev     = node->dev;
    buf->st_rdev    = node->rdev;
    buf->st_ctim = buf->st_atim = buf->st_ctim = buf->st_mtim = (struct timespec){
        .tv_sec = node->createtime / 1000000000ULL, .tv_nsec = node->createtime % 1000000000ULL};
    buf->st_blksize = PAGE_SIZE;
    buf->st_blocks  = (node->size + PAGE_SIZE - 1) / PAGE_SIZE;
    buf->st_ino     = node->inode;
    return SYSCALL_SUCCESS;
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
    case 7: {
        if (arg1 != 0) {
            struct timespec *ts   = (struct timespec *)arg1;
            uint64_t         nano = sched_clock();
            ts->tv_sec            = nano / 1000000000ULL;
            ts->tv_nsec           = nano % 1000000000ULL;
        }
        return SYSCALL_SUCCESS;
    }
    default: logkf("clock not supported(%d)\n", arg0); return SYSCALL_FAULT_(EINVAL);
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
    char  *npath = vfs_cwd_path_build(name);
    size_t ret   = vfs_mkdir(npath) == VFS_STATUS_FAILED ? -1 : 0;
    free(npath);
    return ret;
}

syscall_(lseek) {
    int             fd     = (int)arg0;
    size_t          offset = arg1;
    size_t          whence = arg2;
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

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
    case SEEK_DATA:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        break;
    case SEEK_HOLE:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        return handle->node->size;
    default: return SYSCALL_FAULT_(ENXIO);
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
    memset(comp, 0, complength);
    size_t compIndex = 0;
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

    //int toZero = (nfds + 8) / 8;
    int toZero = (nfds + 7) / 8;
    if (read) memset(read, 0, toZero);
    if (write) memset(write, 0, toZero);
    if (except) memset(except, 0, toZero);

    size_t time = 0;
    if (timeout == NULL) {
        time = -1;
    } else if (timeout->tv_sec == -1 || timeout->tv_usec == -1) {
        time = -1;
    } else
        time = (timeout->tv_sec * 1000 + (timeout->tv_usec + 1000) / 1000);

    size_t res = syscall_poll(comp, compIndex, time, 0, 0, 0, 0);

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
        if ((comp[i].events & POLLPRI && comp[i].revents & POLLPRI)) {
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
                                       .tv_usec = (long)(timeout->tv_nsec + 1000l) / 1000};
    } else {
        timeoutConv = (struct timeval){.tv_sec = (long)-1, .tv_usec = (long)-1};
    }

    size_t ret = syscall_select(nfds, (uint64_t)(uint8_t *)readfds, (uint64_t)(uint8_t *)writefds,
                                (uint64_t)(uint8_t *)exceptfds, (uint64_t)&timeoutConv, 0, 0);
    if (sigmask) syscall_ssetmask(SIG_SETMASK, &origmask, NULL);
    return ret;
}

syscall_(reboot, unsigned int magic1, unsigned int magic2, uint32_t cmd, void *arg) {
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
    if (unlikely(check_user_overflow(buf, size))) { return SYSCALL_FAULT_(EFAULT); }
    fd_file_handle *handle = queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }
    size_t         child_count   = (uint64_t)list_length(handle->node->child);
    struct dirent *dents         = (struct dirent *)buf;
    size_t         max_dents_num = size / sizeof(struct dirent);
    size_t         read_count    = 0;
    uint64_t       offset        = 0;
    list_foreach(handle->node->child, i) {
        if (offset < handle->offset) goto next;
        if (handle->offset >= (child_count * sizeof(struct dirent))) break;
        if (read_count >= max_dents_num) break;
        vfs_node_t child_node      = (vfs_node_t)i->data;
        dents[read_count].d_ino    = child_node->inode;
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
        strncpy(dents[read_count].d_name, child_node->name, 256);
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

    uint64_t ret = syscall_stat(resolved, buf, 0, 0, 0, 0, 0);

    free(resolved);

    return ret;
}

syscall_(statx) {
    int           dirfd    = arg0;
    char         *pathname = (char *)arg1;
    uint64_t      flags    = arg2;
    uint64_t      mask     = arg3;
    struct statx *buff     = (struct statx *)arg4;
    if (unlikely(!pathname || check_user_overflow((uint64_t)pathname, strlen(pathname)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (unlikely(!buff || check_user_overflow((uint64_t)buff, sizeof(struct statx)))) {
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

syscall_(pipe) {
    int     *pipefd = (int *)arg0;
    uint64_t flags  = arg1;

    /* fs/pipefs.c */
    extern vfs_node_t pipefs_root;
    extern int        pipefd_id;
    extern int        pipefs_id;

    if (pipefs_root == NULL) return SYSCALL_FAULT_(ENOSYS);

    char buf[16];
    sprintf(buf, "pipe%d", pipefd_id++);

    vfs_node_t node_input = vfs_node_alloc(pipefs_root, buf);
    node_input->type      = file_pipe;
    node_input->fsid      = pipefs_id;
    node_input->refcount++;
    pipefs_root->mode = 0700;

    sprintf(buf, "pipe%d", pipefd_id++);
    vfs_node_t node_output = vfs_node_alloc(pipefs_root, buf);
    node_output->type      = file_pipe;
    node_output->fsid      = pipefs_id;
    node_output->refcount++;
    pipefs_root->mode = 0700;

    pipe_info_t *info = (pipe_info_t *)malloc(sizeof(pipe_info_t));
    memset(info, 0, sizeof(pipe_info_t));
    info->read_fds            = 1;
    info->write_fds           = 1;
    info->blocking_read.next  = NULL;
    info->blocking_write.next = NULL;
    info->lock                = SPIN_INIT;
    info->read_ptr            = 0;
    info->write_ptr           = 0;
    info->assigned            = 0;

    pipe_specific_t *read_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    read_spec->write           = false;
    read_spec->info            = info;
    read_spec->node            = node_input;

    pipe_specific_t *write_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    write_spec->write           = true;
    write_spec->info            = info;
    write_spec->node            = node_output;

    node_input->handle  = read_spec;
    node_output->handle = write_spec;

    lock_queue     *queue     = get_current_task()->parent_group->file_open;
    fd_file_handle *handle_in = malloc(sizeof(fd_file_handle));
    handle_in->node           = node_input;
    handle_in->offset         = 0;
    handle_in->flags          = flags;
    handle_in->fd             = queue_enqueue(queue, handle_in);

    fd_file_handle *handle_out = malloc(sizeof(fd_file_handle));
    handle_out->node           = node_output;
    handle_out->offset         = 0;
    handle_out->flags          = flags;
    handle_out->fd             = queue_enqueue(queue, handle_out);

    pipefd[0] = handle_in->fd;
    pipefd[1] = handle_out->fd;

    return SYSCALL_SUCCESS;
}

syscall_(unlink) {
    char *name = (char *)arg0;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char      *npath = vfs_cwd_path_build(name);
    vfs_node_t node  = vfs_open(npath);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_none && node->type != file_symlink) {
        vfs_close(node);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 0) {
        node->refcount--;
        return SYSCALL_SUCCESS;
    } else
        ret = vfs_delete(node) == VFS_STATUS_SUCCESS ? SYSCALL_SUCCESS : SYSCALL_FAULT_(ENOENT);
    free(npath);
    return ret;
}

syscall_(rmdir) {
    char *name = (char *)arg0;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char      *n_name = vfs_cwd_path_build(name);
    vfs_node_t node   = vfs_open(n_name);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_dir) {
        vfs_close(node);
        free(n_name);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 0) {
        node->refcount--;
        ret = SYSCALL_SUCCESS;
    } else {
        ret = vfs_delete(node);
    }
    free(n_name);
    return ret;
}

syscall_(unlinkat) {
    int   dirfd = (int)arg0;
    char *name  = (char *)arg1;
    if (check_user_overflow((uint64_t)name, strlen(name))) { return (uint64_t)-EFAULT; }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return -ENOENT;

    uint64_t ret = syscall_unlink((uint64_t)path, 0, 0, 0, 0, 0, regs);

    free(path);

    return ret;
}

syscall_(access, char *filename) {
    struct stat buf;
    return syscall_stat(filename, &buf, 0, 0, 0, 0, regs);
}

syscall_(faccessat) {
    int      dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    if (pathname[0] == '\0') { // by fd
        return 0;
    }
    if (check_user_overflow((uint64_t)pathname, strlen(pathname))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return SYSCALL_FAULT_(ENOENT);

    size_t ret = syscall_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

syscall_(faccessat2) {
    uint64_t dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    uint64_t flag     = arg3;
    if (pathname[0] == '\0') { // by fd
        return 0;
    }
    if (check_user_overflow((uint64_t)pathname, strlen(pathname))) {
        return SYSCALL_FAULT_(EFAULT);
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return SYSCALL_FAULT_(ENOENT);

    size_t ret = syscall_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

syscall_(openat) {
    int      dirfd = arg0;
    char    *name  = (char *)arg1;
    uint64_t flags = arg2;
    uint64_t mode  = arg3;

    if (unlikely(!name || check_user_overflow((uint64_t)name, strlen(name)))) {
        return SYSCALL_FAULT_(EFAULT);
    }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return SYSCALL_FAULT_(ENOMEM);

    uint64_t ret = syscall_open(path, flags, mode, 0, 0, 0, regs);

    free(path);

    return ret;
}

syscall_(getuid) {
    return get_current_user()->uid;
}

syscall_(copy_file_range, int fd_in, uint64_t *off_in, int fd_out, uint64_t *off_out, size_t len,
         uint64_t flags) {
    if (flags != 0) { return SYSCALL_FAULT_(EINVAL); }
    fd_file_handle *src_handle = queue_get(get_current_task()->parent_group->file_open, fd_in);
    fd_file_handle *dst_handle = queue_get(get_current_task()->parent_group->file_open, fd_out);
    if (src_handle == NULL || dst_handle == NULL) { return SYSCALL_FAULT_(EBADF); }
    if (dst_handle->offset >= dst_handle->node->size && dst_handle->node->size > 0)
        return SYSCALL_SUCCESS;
    uint64_t src_offset = off_in ? *off_in : src_handle->offset;
    uint64_t dst_offset = off_out ? *off_out : dst_handle->offset;

    uint64_t length     = src_handle->node->size > len ? len : src_handle->node->size;
    uint8_t *buffer     = (uint8_t *)malloc(length);
    size_t   copy_total = 0;
    if (vfs_read(src_handle->node, buffer, src_offset, length) == (size_t)VFS_STATUS_FAILED) {
        goto errno_;
    }
    if ((copy_total = vfs_write(dst_handle->node, buffer, dst_offset, length)) ==
        (size_t)VFS_STATUS_FAILED) {
        goto errno_;
    }
    vfs_update(dst_handle->node);
    free(buffer);
    dst_handle->offset += copy_total;
    return copy_total;
errno_:
    free(buffer);
    return SYSCALL_FAULT_(EFAULT);
}

syscall_(pread, int fd, uint8_t *buffer) {
    syscall_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return syscall_read(fd, buffer, arg2, 0, 0, 0, regs);
}

syscall_(pwrite, int fd, uint8_t *buffer) {
    syscall_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return syscall_write(fd, buffer, arg2, 0, 0, 0, regs);
}

syscall_(mount, char *dev_name, char *dir_name, char *type, uint64_t flags, void *data) {
    if (dir_name == NULL) return SYSCALL_FAULT_(EINVAL);

    char      *ndir_name = vfs_cwd_path_build(dir_name);
    vfs_node_t dir       = vfs_open((const char *)ndir_name);
    if (!dir) {
        free(ndir_name);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (flags & MS_MOVE) {
        if (flags & (MS_REMOUNT | MS_BIND)) {
            free(ndir_name);
            return SYSCALL_FAULT_(EINVAL);
        }
        char      *old_root_p = vfs_cwd_path_build(dev_name);
        vfs_node_t old_root   = vfs_open(old_root_p);
        free(old_root_p);
        if (old_root == NULL || !old_root->is_mount) return SYSCALL_FAULT_(EINVAL);
        if (dir != rootdir) list_append(dir->parent->child, old_root);
        char *nb       = old_root->name;
        old_root->name = dir->name;
        dir->name      = nb;
        list_append(old_root->parent->child, dir);

        list_delete(old_root->parent->child, old_root);
        if (dir != rootdir)
            list_delete(dir->parent->child, dir);
        else
            rootdir = old_root;

        vfs_node_t parent = dir->parent;
        dir->parent       = old_root->parent;
        old_root->parent  = parent;

        vfs_close(old_root);
        vfs_close(dir);
        return SYSCALL_SUCCESS;
    }

    if (type == NULL) return SYSCALL_FAULT_(EINVAL);

    char            *ndev_name;
    vfs_filesystem_t filesystem = get_filesystem(type);
    if (filesystem == NULL) return SYSCALL_FAULT_(EINVAL);
    if (filesystem->id != 0) {
        ndev_name = (char *)filesystem->id;
        goto mount;
    }

    ndev_name = vfs_cwd_path_build(dev_name);
mount:
    if (vfs_mount((const char *)ndev_name, dir) == VFS_STATUS_FAILED) {
        free(ndir_name);
        free(ndev_name);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(ndir_name);
    if (filesystem->id == 0) free(ndev_name);
    return SYSCALL_SUCCESS;
}

syscall_(ftruncate) {
    return SYSCALL_SUCCESS;
}

syscall_(setpgid) {
    pid_t pid     = arg0;
    pid_t pgid    = arg1;
    pcb_t process = pid == 0 ? get_current_task()->parent_group : found_pcb(pid);
    if (process == NULL || process->status == DEATH) { return SYSCALL_FAULT_(ESRCH); }
    if (pgid == 0) { pgid = process->pgid; }
    process->pgid = pgid;
    return SYSCALL_SUCCESS;
}

syscall_(getpgid) {
    size_t pid     = arg0;
    pcb_t  process = pid == 0 ? get_current_task()->parent_group : found_pcb(pid);
    if (process == NULL || process->status == DEATH) { return SYSCALL_FAULT_(ESRCH); }
    return process->pgid;
}

syscall_(geteuid) {
    return get_current_task()->parent_group->user->uid;
}

syscall_(setuid) {
    int uid = arg0;
    if (uid > 65535) return SYSCALL_FAULT_(EINVAL);
    if (get_current_task()->parent_group->user->uid == 0) return SYSCALL_FAULT_(EACCES);
    get_current_task()->parent_group->user->uid = uid;
    return SYSCALL_SUCCESS;
}

syscall_(setgid) {
    // TODO GID设置不支持
    return SYSCALL_SUCCESS;
}

syscall_(getegid) {
    //TODO EGID获取不支持
    return 0;
}

syscall_(getgroups) {
    int  count    = arg0;
    int *gid_list = (int *)arg1;
    if (count > 0) {
        gid_list[0] = 0;
        return 1;
    }
    return 0;
}

syscall_(rename) {
    char *oldpath = (char *)arg0;
    char *newpath = (char *)arg1;
    if (!oldpath || !newpath) return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow((uint64_t)oldpath, strlen(oldpath))) { return SYSCALL_FAULT_(EFAULT); }
    if (check_user_overflow((uint64_t)newpath, strlen(newpath))) { return SYSCALL_FAULT_(EFAULT); }
    char      *noldpath = vfs_cwd_path_build(oldpath);
    char      *nnewpath = vfs_cwd_path_build(newpath);
    vfs_node_t oldnode  = vfs_open(noldpath);
    if (!oldnode) {
        free(noldpath);
        free(nnewpath);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_node_t newnode = vfs_open(nnewpath);
    if (newnode) { vfs_delete(newnode); }
    size_t     ret        = vfs_rename(oldnode, nnewpath) == VFS_STATUS_SUCCESS ? SYSCALL_SUCCESS
                                                                                : SYSCALL_FAULT_(ENOENT);
    char      *parent     = get_parent_path(nnewpath);
    vfs_node_t parent_dir = vfs_open(parent);
    if (!parent_dir) {
        free(parent);
        ret = SYSCALL_FAULT_(ENOENT);
        goto end_rename;
    }
    vfs_close(parent_dir); // 更新父目录的信息
    free(parent);
end_rename:
    free(noldpath);
    free(nnewpath);
    return ret;
}

syscall_(symlink) {
    char *name = (char *)arg0;
    char *new  = (char *)arg1;
    if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_symlink(name, new);

    return ret;
}

syscall_(link) {
    char *name = (char *)arg0;
    char *new  = (char *)arg1;
    if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_link(name, new);

    return ret;
}

syscall_(socket, int domain, int type, int protocol) {
    return syscall_net_socket(domain, type, protocol);
}

syscall_(umount2) {
    char   *path   = normalize_path((char *)arg0);
    int     flags  = arg1;
    errno_t status = vfs_unmount(path);
    if (status == VFS_STATUS_FAILED) status = SYSCALL_FAULT_(EBUSY);
    free(path);
    return status;
}

syscall_(readlink) {
    char    *path = (char *)arg0;
    char    *buf  = (char *)arg1;
    uint64_t size = arg2;
    if (path == NULL || buf == NULL || size == 0) { return SYSCALL_FAULT_(EINVAL); }
    if (check_user_overflow((uint64_t)buf, size)) { return SYSCALL_FAULT_(EFAULT); }

    vfs_node_t node = vfs_open(path);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    if (!node->linkto) return SYSCALL_FAULT_(EINVAL);

    return vfs_readlink(node, buf, (size_t)size);
}

syscall_(sysinfo) {
    struct sysinfo *info = (struct sysinfo *)arg0;
    if (check_user_overflow((uint64_t)info, sizeof(struct sysinfo))) return SYSCALL_FAULT_(EFAULT);
    memset(info, 0, sizeof(struct sysinfo));
    info->freeram  = get_available_memory();
    info->totalram = get_all_memory();
    info->mem_unit = 1;
    info->procs    = pgb_queue->size;
    return SYSCALL_SUCCESS;
}

syscall_(getgid) {
    return get_current_task()->parent_group->pgid;
}

syscall_(fsopen) {
    char    *fsname = (char *)arg0;
    uint32_t flags  = arg1;

    vfs_filesystem_t filesystem = NULL;
    vfs_filesystem_t pos, n;
    llist_for_each(pos, n, &fs_metadata_list, node) {
        if (strcmp(pos->name, fsname) != 0) {
            filesystem = pos;
            break;
        }
    }

    if (filesystem == NULL) return SYSCALL_FAULT_(ENOENT);

    vfs_node_t node = vfs_node_alloc(NULL, NULL);
    node->type      = file_none;
    node->handle    = filesystem;
    node->refcount++;
    node->size = 0;

    fd_file_handle *handle = calloc(1, sizeof(fd_file_handle));
    handle->node           = node;
    handle->fd             = queue_enqueue(get_current_task()->parent_group->file_open, handle);
    handle->flags          = flags;
    return handle->fd;
}

syscall_(getppid) {
    return get_current_task()->parent_group->parent_task->pid;
}

syscall_(mincore, uint64_t addr, uint64_t size, uint64_t vec) {
    if (check_user_overflow(addr, size)) { return SYSCALL_FAULT_(EFAULT); }

    if (size == 0) { return SYSCALL_SUCCESS; }

    uint64_t start_page = addr & (~(PAGE_SIZE - 1));
    uint64_t end_page   = (addr + size - 1) & (~(PAGE_SIZE - 1));
    uint64_t num_pages  = ((end_page - start_page) / PAGE_SIZE) + 1;

    if (check_user_overflow(vec, num_pages)) { return SYSCALL_FAULT_(EFAULT); }

    spin_lock(mm_op_lock);
    uint64_t current_addr = start_page;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t phys_addr = page_virt_to_phys(current_addr);

        uint8_t resident = (phys_addr != 0) ? 1 : 0;

        memcpy((void *)(vec + i), &resident, sizeof(uint8_t));

        current_addr += PAGE_SIZE;
    }

    spin_unlock(mm_op_lock);
    return SYSCALL_SUCCESS;
}

syscall_(pivot_root, char *new_root, char *put_old) {
    //TODO 未经测试和验证的实现
    vfs_node_t nroot = vfs_open(new_root);
    if (nroot == NULL) return SYSCALL_FAULT_(ENOENT);
    if (!nroot->is_mount) {
        vfs_close(nroot);
        goto error_pr;
    }
    vfs_node_t oroot = vfs_open(put_old);
    if (oroot == NULL) return SYSCALL_FAULT_(ENOENT);
    if (!(oroot->type & file_dir)) goto error_pr;
    if (list_length(oroot->child) > 0) goto error_pr;
    if (oroot->root != nroot) goto error_pr;
    vfs_node_t src_root = get_rootdir();
    set_rootdir(nroot);
    oroot->handle = src_root->handle;
    oroot->child  = src_root->child;
    return SYSCALL_SUCCESS;
error_pr:
    return SYSCALL_FAULT_(EINVAL);
}

syscall_(chroot, char *path) {
    if (path == NULL) return SYSCALL_FAULT_(EINVAL);
    char      *npath   = vfs_cwd_path_build(path);
    pcb_t      process = get_current_task()->parent_group;
    vfs_node_t node    = vfs_open(npath);
    free(npath);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (process->proc_root != NULL) vfs_close(process->proc_root);
    process->proc_root = node;
    return SYSCALL_SUCCESS;
}

syscall_(statfs, char *path, struct statfs *buf) {
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    vfs_filesystem_t filesystem = get_filesystem_node(node);
    if (filesystem == NULL) return SYSCALL_FAULT_(EINVAL);
    buf->f_type = filesystem->magic;
    return SYSCALL_SUCCESS;
}

syscall_(bind, int sockfd, const struct sockaddr_un *addr, socklen_t addrlen) {
    return syscall_net_bind(sockfd, addr, addrlen);
}

syscall_(listen, int sockfd, int backlog) {
    return syscall_net_listen(sockfd, backlog);
}

syscall_(accept, int sockfd, struct sockaddr_un *addr, socklen_t *addrlen, uint64_t flags) {
    return syscall_net_accept(sockfd, addr, addrlen, flags);
}

syscall_(connect, int sockfd, const struct sockaddr_un *addr, socklen_t addrlen) {
    return syscall_net_connect(sockfd, addr, addrlen);
}

syscall_(send, int sockfd, void *buff, size_t len, int flags, struct sockaddr_un *dest_addr,
         socklen_t addrlen) {
    return syscall_net_send(sockfd, buff, len, flags, dest_addr, addrlen);
}

// clang-format off
syscall_t syscall_handlers[MAX_SYSCALLS] = {
    [SYSCALL_EXIT]        = (syscall_t)syscall_exit,
    [SYSCALL_EXIT_GROUP]  = (syscall_t)syscall_exit_group,
    [SYSCALL_OPEN]        = (syscall_t)syscall_open,
    [SYSCALL_CLOSE]       = (syscall_t)syscall_close,
    [SYSCALL_WRITE]       = (syscall_t)syscall_write,
    [SYSCALL_READ]        = (syscall_t)syscall_read,
    [SYSCALL_WAITPID]     = (syscall_t)syscall_waitpid,
    [SYSCALL_MMAP]        = (syscall_t)syscall_mmap,
//TODO [SYSCALL_SIGNAL]      = syscall_signal,
    [SYSCALL_SIGRET]      = (syscall_t)syscall_sigret,
    [SYSCALL_GETPID]      = (syscall_t)syscall_getpid,
    [SYSCALL_PRCTL]       = (syscall_t)syscall_prctl,
    [SYSCALL_STAT]        = (syscall_t)syscall_stat,
    [SYSCALL_CLONE]       = (syscall_t)syscall_clone,
    [SYSCALL_ARCH_PRCTL]  = (syscall_t)syscall_arch_prctl,
    [SYSCALL_YIELD]       = (syscall_t)syscall_yield,
    [SYSCALL_UNAME]       = (syscall_t)syscall_uname,
    [SYSCALL_NANO_SLEEP]  = (syscall_t)syscall_nano_sleep,
    [SYSCALL_IOCTL]       = (syscall_t)syscall_ioctl,
    [SYSCALL_WRITEV]      = (syscall_t)syscall_writev,
    [SYSCALL_READV]       = (syscall_t)syscall_readv,
    [SYSCALL_MUNMAP]      = (syscall_t)syscall_munmap,
    [SYSCALL_MREMAP]      = (syscall_t)syscall_mremap,
    [SYSCALL_GETCWD]      = (syscall_t)syscall_getcwd,
    [SYSCALL_CHDIR]       = (syscall_t)syscall_chdir,
    [SYSCALL_POLL]        = (syscall_t)syscall_poll,
    [SYSCALL_SETID_ADDR]  = (syscall_t)syscall_set_tid_address,
    [SYSCALL_G_AFFINITY]  = (syscall_t)syscall_sched_getaffinity,
    [SYSCALL_RT_SIGMASK]  = (syscall_t)syscall_rt_sigprocmask,
    [SYSCALL_FCNTL]       = (syscall_t)syscall_fcntl,
    [SYSCALL_DUP]         = (syscall_t)syscall_dup,
    [SYSCALL_DUP2]        = (syscall_t)syscall_dup2,
    [SYSCALL_SIGALTSTACK] = (syscall_t)syscall_sigaltstack,
    [SYSCALL_SIGACTION]   = (syscall_t)syscall_sigaction,
    [SYSCALL_FORK]        = (syscall_t)syscall_fork,
    [SYSCALL_FUTEX]       = (syscall_t)syscall_futex,
    [SYSCALL_GET_TID]     = (syscall_t)syscall_get_tid,
    [SYSCALL_MPROTECT]    = (syscall_t)syscall_mprotect,
    [SYSCALL_VFORK]       = (syscall_t)syscall_vfork,
    [SYSCALL_EXECVE]      = (syscall_t)syscall_execve,
    [SYSCALL_FSTAT]       = (syscall_t)syscall_fstat,
    [SYSCALL_C_GETTIME]   = (syscall_t)syscall_clock_gettime,
    [SYSCALL_C_GETRES]    = (syscall_t)syscall_clock_getres,
    [SYSCALL_SIGSUSPEND]  = (syscall_t)syscall_sigsuspend,
    [SYSCALL_MKDIR]       = (syscall_t)syscall_mkdir,
    [SYSCALL_LSEEK]       = (syscall_t)syscall_lseek,
    [SYSCALL_PSELECT6]    = (syscall_t)syscall_pselect6,
    [SYSCALL_SELECT]      = (syscall_t)syscall_select,
    [SYSCALL_REBOOT]      = (syscall_t)syscall_reboot,
    [SYSCALL_GETDENTS64]  = (syscall_t)syscall_getdents,
    [SYSCALL_STATX]       = (syscall_t)syscall_statx,
    [SYSCALL_NEWFSTATAT]  = (syscall_t)syscall_newfstatat,
    [SYSCALL_LSTAT]       = (syscall_t)syscall_stat,
    [SYSCALL_PIPE]        = (syscall_t)syscall_pipe,
    [SYSCALL_PIPE2]       = (syscall_t)syscall_pipe,
    [SYSCALL_UNLINK]      = (syscall_t)syscall_unlink,
    [SYSCALL_RMDIR]       = (syscall_t)syscall_rmdir,
    [SYSCALL_UNLINKAT]    = (syscall_t)syscall_unlinkat,
    [SYSCALL_FACCESSAT]   = (syscall_t)syscall_faccessat,
    [SYSCALL_FACCESSAT2]  = (syscall_t)syscall_faccessat2,
    [SYSCALL_ACCESS]      = (syscall_t)syscall_access,
    [SYSCALL_OPENAT]      = (syscall_t)syscall_openat,
    [SYSCALL_GETUID]      = (syscall_t)syscall_getuid,
    [SYSCALL_CP_F_RANGE]  = (syscall_t)syscall_copy_file_range,
    [SYSCALL_PREAD]       = (syscall_t)syscall_pread,
    [SYSCALL_PWRITE]      = (syscall_t)syscall_pwrite,
    [SYSCALL_MOUNT]       = (syscall_t)syscall_mount,
    [SYSCALL_FTRUNCATE]   = (syscall_t)syscall_ftruncate,
    [SYSCALL_SETPGID]     = (syscall_t)syscall_setpgid,
    [SYScall_GETPGID]     = (syscall_t)syscall_getpgid,
    [SYSCALL_GETEUID]     = (syscall_t)syscall_geteuid,
    [SYSCALL_SETUID]      = (syscall_t)syscall_setuid,
    [SYSCALL_SETGID]      = (syscall_t)syscall_setgid,
    [SYSCALL_GETEGID]     = (syscall_t)syscall_getegid,
    [SYSCALL_GETGROUPS]   = (syscall_t)syscall_getgroups,
    [SYSCALL_RENAME]      = (syscall_t)syscall_rename,
    [SYSCALL_SYMLINK]     = (syscall_t)syscall_symlink,
    [SYSCALL_LINK]        = (syscall_t)syscall_link,
    [SYSCALL_SOCKET]      = (syscall_t)syscall_socket,
    [SYSCALL_UMOUNT2]     = (syscall_t)syscall_umount2,
    [SYSCALL_READLINK]    = (syscall_t)syscall_readlink,
    [SYSCALL_SYSINFO]     = (syscall_t)syscall_sysinfo,
    [SYSCALL_GETGID]      = (syscall_t)syscall_getgid,
    [SYSCALL_FSOPEN]      = (syscall_t)syscall_fsopen,
    [SYSCALL_GETPPID]     = (syscall_t)syscall_getppid,
    [SYSCALL_MINCORE]     = (syscall_t)syscall_mincore,
    [SYSCALL_PIVOT_ROOT]  = (syscall_t)syscall_pivot_root,
    [SYSCALL_CHROOT]      = (syscall_t)syscall_chroot,
    [SYSCALL_STATFS]      = (syscall_t)syscall_statfs,
    [SYSCALL_BIND]        = (syscall_t)syscall_bind,
    [SYSCALL_LISTEN]      = (syscall_t)syscall_listen,
    [SYSCALL_ACCEPT]      = (syscall_t)syscall_accept,
    [SYSCALL_CONNECT]     = (syscall_t)syscall_connect,

    [SYSCALL_MEMINFO]     = (syscall_t)syscall_cp_meminfo,
    [SYSCALL_CPUINFO]     = (syscall_t)syscall_cp_cpuinfo,
};
// clang-format on

syscall_(cp_meminfo) {
    struct cpos_meminfo *meminfo = (struct cpos_meminfo *)arg0;
    if (meminfo == NULL) return SYSCALL_FAULT_(EINVAL);
    meminfo->all_size  = get_all_memory();
    meminfo->available = get_available_memory();
    meminfo->commit    = get_commit_memory();
    meminfo->reserved  = get_reserved_memory();
    meminfo->used      = get_used_memory();
    return SYSCALL_SUCCESS;
}

syscall_(cp_cpuinfo) {
    extern uint64_t cpu_count;

    struct cpos_cpuinfo *cpuinfo = (struct cpos_cpuinfo *)arg0;
    if (cpuinfo == NULL) return SYSCALL_FAULT_(EINVAL);
    cpu_t cpu = get_cpu_info();
    strcpy(cpuinfo->vendor, cpu.vendor);
    strcpy(cpuinfo->model_name, cpu.model_name);
    cpuinfo->phys_bits = cpu.phys_bits;
    cpuinfo->virt_bits = cpu.virt_bits;
    cpuinfo->cores     = cpu_count;

    cpuinfo->flags = 0;
    if (x2apic_mode_supported()) cpuinfo->flags |= CPUINFO_X2APIC;
    if (cpuid_has_sse()) cpuinfo->flags |= CPUINFO_SSE;
    if (cpu_has_rdtsc()) cpuinfo->flags |= CPUINFO_RDTSC;

    return SYSCALL_SUCCESS;
}

USED void syscall_handler(struct syscall_regs *regs, uint64_t user_regs) { // syscall 指令处理
    regs->rip    = regs->rcx;
    regs->rflags = regs->r11;
    regs->cs     = (0x20 | 0x3);
    regs->ss     = (0x18 | 0x3);
    regs->ds     = (0x18 | 0x3);
    regs->es     = (0x18 | 0x3);
    regs->rsp    = user_regs;

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
    if (likely(syscall_id < MAX_SYSCALLS && syscall_handlers[syscall_id] != NULL)) {
        open_interrupt;
        //logkf("syscall(%llu): call\n", syscall_id);
        regs->rax = ((syscall_t)syscall_handlers[syscall_id])(regs->rdi, regs->rsi, regs->rdx,
                                                              regs->r10, regs->r8, regs->r9, regs);
        //logkf("syscall(%llu): call\n", syscall_id);
        close_interrupt;
    } else {
        if (unlikely(syscall_id != 12)) logkf("Syscall(%d) cannot implemented.\n", syscall_id);
        regs->rax = SYSCALL_FAULT;
    }

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
