#include "exec/elf_load.h"
#include "bootarg.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

void load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user,
                  uint64_t offset, uint64_t *load_start) {
    size_t hi = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000) + offset;
    size_t lo = PADDING_DOWN(phdr->p_paddr, 0x1000) + offset;
    if (load_start != NULL) {
        if (lo < *load_start) { *load_start = lo; }
    }
    uint64_t flags =
#if defined(__x86_64__) || defined(__amd64__)
        PTE_PRESENT | PTE_WRITEABLE;
#elif defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
        ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_WRITE | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_EXEC;
#elif defined(__loongarch__) || defined(__loongarch64)
        ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_DIRTY;
#endif

    if (is_user)
#if defined(__x86_64__) || defined(__amd64__)
        flags |= PTE_USER;
#elif defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
        flags |= ARCH_PT_FLAG_USER;
#elif defined(__loongarch__) || defined(__loongarch64)
        flags |= ARCH_PT_FLAG_USER;
#endif
    if ((phdr->p_flags & PF_R) && !(phdr->p_flags & PF_W)) {
        for (size_t i = lo; i < hi; i += 0x1000) {
            page_map_to(directory, i, alloc_frames(1), flags);
        }
    } else
        for (size_t i = lo; i < hi; i += 0x1000) {
            page_map_to(directory, i, alloc_frames(1), flags);
        }
    uint64_t          p_vaddr  = (uint64_t)phdr->p_vaddr + offset;
    uint64_t          p_filesz = (uint64_t)phdr->p_filesz;
    uint64_t          p_memsz  = (uint64_t)phdr->p_memsz;
    page_directory_t *dir      = get_current_directory();
    switch_context_directory(directory);
    memcpy((void *)p_vaddr, elf + phdr->p_offset, p_memsz);

    if (p_memsz > p_filesz) { // 这个是bss段
        memset((void *)(p_vaddr + p_filesz), 0, p_memsz - p_filesz);
    }
    switch_context_directory(dir);
}

bool mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory,
                       bool is_user, uint64_t offset, uint64_t *load_start, uint64_t *load_size) {
    size_t i = 0;
    while (i < ehdr->e_phnum && phdrs[i].p_type != PT_LOAD) {
        i++;
    }

    if (i == ehdr->e_phnum) { return false; }

    uint64_t load_min = 0xffffffffffffffff;
    uint64_t load_max = 0x0000000000000000;

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_segment(&phdrs[i], (void *)ehdr, directory, is_user, offset, load_start);
            if (phdrs[i].p_vaddr + offset + phdrs[i].p_memsz > load_max)
                load_max = phdrs[i].p_vaddr + offset + phdrs[i].p_memsz;
            if (phdrs[i].p_vaddr + offset < load_min) load_min = phdrs[i].p_vaddr + offset;
        }
    }

    if (load_size) { *load_size = load_max - load_min; }

    return true;
}

bool is_dynamic(Elf64_Ehdr *ehdr) {
    if (ehdr->e_type != ET_DYN) { return false; }
    if (ehdr->e_phnum == 0 || ehdr->e_phoff == 0) { return false; }
    return true;
}

void *load_executor_elf(uint8_t *data, page_directory_t *dir, uint64_t offset, uint64_t *load_start,
                        pcb_t process) {
    if (data == NULL) return NULL;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    if (!arch_elf_test_head(ehdr)) { return NULL; }
    Elf64_Phdr       *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    page_directory_t *cur   = get_current_directory();
    switch_context_directory(dir);
    size_t load_size = 0;
    if (!mmap_phdr_segment(ehdr, phdrs, dir, true, offset, load_start, &load_size)) { return NULL; }
    // VMA
    if (process != NULL) {
        vma_t *ld_so_vma = vma_alloc();

        ld_so_vma->vm_start  = *load_start;
        ld_so_vma->vm_end    = *load_start + load_size;
        ld_so_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

        ld_so_vma->vm_type = VMA_TYPE_ANON;
        ld_so_vma->vm_name = strdup(process->name);
        vma_insert(&process->vma_manager, ld_so_vma);
    }
    switch_context_directory(cur);
    return (void *)ehdr->e_entry;
}

void *load_interpreter_elf(uint8_t *data, page_directory_t *dir, uint64_t *load_start,
                           uint8_t **link_data, size_t *link_size) {

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    if (!arch_elf_test_head(ehdr)) { return NULL; }
    Elf64_Phdr *phdrs            = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    char       *interpreter_name = NULL;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_INTERP) {
            interpreter_name = ((char *)ehdr + phdrs[i].p_offset);
            logkf("load interpreter: %s\n", interpreter_name);
        }
    }
    if (interpreter_name == NULL) return NULL;
    vfs_node_t inter_file = vfs_open(interpreter_name);
    if (inter_file == NULL) return NULL;
    Elf64_Ehdr *inter_ehdr = malloc(inter_file->size);
    if (vfs_read(inter_file, inter_ehdr, 0, inter_file->size) == (size_t)-1) {
        vfs_close(inter_file);
        free(inter_ehdr);
        *link_data = NULL;
        *link_size = 0;
        return NULL;
    }
    void *start =
        load_executor_elf((uint8_t *)inter_ehdr, dir, INTERPRETER_BASE_ADDR, load_start, NULL);
    *link_data = (uint8_t *)inter_ehdr;
    *link_size = inter_file->size;
    vfs_close(inter_file);
    return start;
}

void launch_init_process() {
    const char *cmdline = boot_get_cmdline_param("init");
    vfs_node_t  node    = vfs_open("/bin/sh");
    if (node == NULL) {
        kwarn("Cannot open init file.");
        return;
    }
    pid_t init_pid = create_process("/bin/sh", NULL, CLONE_VM);
    if (init_pid == -1) {
        kerror("Cannot create init process\n");
        return;
    }
    vfs_node_t dev = vfs_open("/dev");
    if (vfs_mount(NULL, "devtmpfs", dev) != EOK) {
        kerror("Cannot mount devtmpfs");
        return;
    }
    pcb_t init_process    = found_pcb(init_pid);
    init_process->exec    = node;
    init_process->envp    = malloc(4 * sizeof(char *));
    init_process->envp[3] = NULL;
    init_process->envc    = 3;
    init_process->envp[0] = strdup("PWD=/");
    init_process->envp[1] = strdup("HOME=/root");
    init_process->envp[2] = strdup("TERM=linux");

    char *argv[] = {
        "/bin/sh",
        "/init",
        NULL,
    };
    init_process->cmdline = build_proc_cmdline(argv, &init_process->cl_length);

    fd_t *stdout = calloc(1, sizeof(fd_t));
    stdout->node = vfs_open("/dev/stdout");
    fd_t *stderr = calloc(1, sizeof(fd_t));
    stderr->node = vfs_open("/dev/stderr");
    fd_t *stdin  = calloc(1, sizeof(fd_t));
    stdin->node  = vfs_open("/dev/stdin");

    if (stdout->node == NULL || stderr->node == NULL || stdin->node == NULL) {
        free(stdout);
        free(stderr);
        free(stdin);
        kerror("Cannot open stdout stderr stdin");
        return;
    }

    stdin->fd  = add_fd(init_process->fdts, stdin);
    stdout->fd = add_fd(init_process->fdts, stdout);
    stderr->fd = add_fd(init_process->fdts, stderr);

    create_kernel_thread("main", (void *)arch_switch_to_user_mode, NULL, init_process,
                         NICE_TO_PRIO(0));

    int exit_code = waitpid(init_pid, &init_pid);
    kwarn("Init process exit, code:%d", exit_code);
}
