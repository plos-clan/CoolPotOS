#include "elf_util.h"
#include "frame.h"
#include "krlibc.h"

bool elf_test_head(Elf64_Ehdr *ehdr) {
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_version != EV_CURRENT || ehdr->e_ehsize != sizeof(Elf64_Ehdr) ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return false;
    }

    switch (ehdr->e_machine) {
    case EM_X86_64:
    case EM_386: break;
    default: return false;
    }

    return true;
}

void load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user) {
    size_t   hi    = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000);
    size_t   lo    = PADDING_DOWN(phdr->p_paddr, 0x1000);
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
    uint64_t p_vaddr  = (uint64_t)phdr->p_vaddr;
    uint64_t p_filesz = (uint64_t)phdr->p_filesz;
    uint64_t p_memsz  = (uint64_t)phdr->p_memsz;
    memcpy((void *)phdr->p_vaddr, elf + phdr->p_offset, phdr->p_memsz);

    if (p_memsz > p_filesz) { // 这个是bss段
        memset((void *)(p_vaddr + p_filesz), 0, p_memsz - p_filesz);
    }
}

bool mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory,
                       bool is_user) {
    size_t i = 0;
    while (i < ehdr->e_phnum && phdrs[i].p_type != PT_LOAD) {
        i++;
    }

    if (i == ehdr->e_phnum) { return false; }

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_segment(&phdrs[i], (void *)ehdr, directory, is_user);
        }
    }

    return true;
}

elf_start load_executor_elf(cp_module_t *file, page_directory_t *dir) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)file->data;
    if (!elf_test_head(ehdr)) { return NULL; }
    Elf64_Phdr       *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    page_directory_t *cur   = get_current_directory();
    switch_process_page_directory(dir);
    if (!mmap_phdr_segment(ehdr, phdrs, dir, true)) { return NULL; }
    switch_process_page_directory(cur);
    return (elf_start)ehdr->e_entry;
}
