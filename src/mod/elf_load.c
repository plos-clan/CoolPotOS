#include "exec/elf_load.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/task.h"

void load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user,
                  uint64_t offset, uint64_t *load_start) {
    size_t hi = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000) + offset;
    size_t lo = PADDING_DOWN(phdr->p_paddr, 0x1000) + offset;
    if (load_start != NULL) {
        if (lo < *load_start) { *load_start = lo; }
    }
    uint64_t flags = PTE_PRESENT | PTE_WRITEABLE;
    if (is_user) flags |= PTE_USER;
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

void *load_executor_elf(uint8_t *data, page_directory_t *dir, uint64_t offset,
                            uint64_t *load_start, pcb_t process) {
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
    return (void*)ehdr->e_entry;
}
