#include "errno.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/lazy_alloc.h"
#include "mem/page.h"
#include "metadata.h"
#include "syscall.h"
#include "task/task.h"

spin_t mm_op_lock = SPIN_INIT;

syscall_(mmap, uint64_t addr, size_t length, uint64_t prot, uint64_t flags, int fd,
         uint64_t offset) {

    addr = addr & (~(PAGE_SIZE - 1));

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    if (check_user_overflow(addr, aligned_len)) { return -EFAULT; }

    if (aligned_len == 0) { return SYSCALL_FAULT_(EINVAL); }
    pcb_t process = get_current_task()->process;

    vma_manager_t *mgr        = &process->vma_manager;
    uint64_t       start_addr = 0;
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
        if (get_fd(process->fdts, fd) == NULL) return SYSCALL_FAULT_(EBADF);
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
        vma->vm_offset = (int64_t)offset;
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
        uint64_t ret = (uint64_t)vfs_map(get_fd(process->fdts, fd)->node, start_addr, aligned_len,
                                         prot, flags, offset);
        spin_unlock(mm_op_lock);
        return ret;
    }

    uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

    if (prot != PROT_NONE) {
        if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
        if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(prot & PROT_EXEC)) pt_flags |= PTE_NO_EXECUTE;
    }

    lazy_infoalloc(process, start_addr, aligned_len, pt_flags, flags);
    spin_unlock(mm_op_lock);

    return start_addr;

    return addr;
}

syscall_(munmap, uint64_t addr, size_t size) {
    if (size == 0) return EOK;

    addr = addr & (~(PAGE_SIZE - 1));
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (check_user_overflow(addr, size)) { return -EFAULT; }

    vma_manager_t *mgr  = &get_current_task()->process->vma_manager;
    vma_t         *vma  = mgr->vma_list;
    vma_t         *next = NULL;

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

    unmap_virtual_page(get_current_task()->process, addr, size);
    unmap_page_range(get_current_directory(), addr, size);
    return EOK;
}

syscall_(mremap, uint64_t old_addr, uint64_t old_size, uint64_t new_size, uint64_t flags,
         uint64_t new_addr) {
    old_addr = old_addr & (~(PAGE_SIZE - 1));
    new_addr = new_addr & (~(PAGE_SIZE - 1));
    old_size = (old_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    new_size = (new_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    vma_manager_t *mgr = &get_current_task()->process->vma_manager;

    vma_t *vma = vma_find(mgr, (unsigned long)old_addr);
    if (!vma || vma->vm_start != (unsigned long)old_addr) { return SYSCALL_FAULT_(EINVAL); }

    uint64_t old_addr_phys = arch_virt_to_phys(old_addr);

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
